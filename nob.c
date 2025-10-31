#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build"

#define streq(a, b) (strcmp(a, b) == 0)

void usage(const char *program) {
  printf("Usage: %s [COMMANDS] [OPTIONS]\n", program);
  printf("Commands:\n");
  printf("    run        ---        Execute executable program after compiling\n");
  printf("    build      ---        Force the building of program\n");
  printf("Options:\n");
  printf("    -g         ---        Build with debug information\n");
  printf("    -ts        ---        Transpile typescript with bun\n");
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *program_name = shift(argv, argc);
  bool run_requested = false;
  bool build_demanded = false;
  bool debug_build = false;
  bool bun_build = false;
  while (argc > 0) {
    const char *arg = shift(argv, argc);

    if (streq(arg, "run")) {
      run_requested = true;
      continue;
    }

    if (streq(arg, "build")) {
      build_demanded = true;
      continue;
    }

    if (streq(arg, "-g")) {
      debug_build = true;
      continue;
    }

    if (streq(arg, "-ts")) {
      bun_build = true;
      continue;
    }
    
    nob_log(ERROR, "Unknown argument provided to build system: %s", arg);
    usage(program_name);
    return 1;
  }

  Cmd cmd = {0};
  const char *output_path;
  if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;


  output_path = BUILD_FOLDER"/dieq-alloc.wasm";
  if (build_demanded || needs_rebuild1(output_path, "./wasm.c")) {
    // clang -o build/dieq.wasm wasm.c --target=wasm32 -Wl,--export=all -Wl,--allow-undefined -nostdlib -Wl,--no-entry
    cmd_append(&cmd, "clang");
    cmd_append(&cmd, "--target=wasm32", "-nostdlib", "-Wl,--allow-undefined", "-Wl,--no-entry");
    cmd_append(&cmd, "-Wl,--export=__heap_base", "-Wl,--export=__heap_end");
    cmd_append(&cmd, "-Wl,--export=dieq_global_setup");
    cmd_append(&cmd, "-Wl,--export=dieq_alloc", "-Wl,--export=dieq_free", "-Wl,--export=dieq_realloc");
    cmd_append(&cmd, "-Wl,--export=foo");
    // cmd_append(&cmd, "-Wl,--export-all");
    nob_cc_output(&cmd, output_path);
    nob_cc_inputs(&cmd, "./wasm.c");
    if (debug_build) {
      cmd_append(&cmd, "-g"); // Embed DWARF debug info
      cmd_append(&cmd, "-O0", "-fno-inline"); // Make sure generated code and source code map 1 to 1
      cmd_append(&cmd, "-gcolumn-info"); // Better breakpoints
      cmd_append(&cmd, "-fno-omit-frame-pointer"); // Full backtraces
      cmd_append(&cmd, "-fsanitize=undefined");
    }
    if (!cmd_run(&cmd)) return 1;

    if (!copy_file(output_path, "dieq-alloc.wasm")) return 1;
  }

  if (bun_build) {
    Nob_File_Paths children = {0};
    if (!build_demanded) {
      if (!read_entire_dir("./ts-src/", &children)) return 1;
      for (size_t i = 0; i < children.count; ++i) {
        children.items[i] = temp_sprintf("./ts-src/%s", children.items[i]);
      }
    }
    if (build_demanded || needs_rebuild(output_path, children.items, children.count)) {
      output_path = "main.js";
      // bun build --outfile=main.js ./main.ts
      cmd_append(&cmd, "bun", "build");
      cmd_append(&cmd, "--outfile=main.js", "./ts-src/main.ts");
      if (!cmd_run(&cmd)) return 1;
    }
  }

  output_path = BUILD_FOLDER"/dieq-alloc";
  if (build_demanded || needs_rebuild1(output_path, "./main.c")) {
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, output_path);
    nob_cc_inputs(&cmd, "main.c");

    if (debug_build) {
      cmd_append(&cmd, "-ggdb");
      cmd_append(&cmd, "-fsanitize=address,undefined");
    }
    if (!cmd_run(&cmd)) return 1;
  }

  if (run_requested) {
    cmd_append(&cmd, output_path);
    if (!cmd_run(&cmd)) return 1;
  }

  return 0;
}
