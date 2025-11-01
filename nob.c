#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build"

#define streq(a, b) (strcmp(a, b) == 0)

typedef struct {
  bool run;
  bool force_build;
  bool debug_info;
  bool ts_build;
} Flags;

struct {
  Flags flags;

  struct {
    const char **items;
    size_t count;
    size_t capacity;
    bool build_all;
  } examples;
} opt = {0};

void usage(const char *program) {
  printf("Usage: %s [COMMANDS] [OPTIONS]\n", program);
  printf("Commands:\n");
  printf("    run        ---        Execute executable program after compiling\n");
  printf("    build      ---        Force the building of program\n");
  printf("Options:\n");
  printf("    -g         ---        Build with debug information\n");
  printf("    -ts        ---        Transpile typescript with bun\n");
}

bool build_example(Cmd *cmd, const char *example_name) {
  bool result = true;

  const char *output_path = temp_sprintf(BUILD_FOLDER"/%s", example_name);
  const char *input_paths[] = {
    temp_sprintf("./examples/%s.c", example_name),
    "./dieq.h",
  };
  if (opt.flags.force_build || needs_rebuild(output_path, input_paths, 2)) {
    nob_cc(cmd);
    nob_cc_flags(cmd);
    nob_cc_output(cmd, output_path);
    cmd_append(cmd, "-lSDL3", "-lm");
    cmd_append(cmd, "-I.");
    if (opt.flags.debug_info) {
      cmd_append(cmd, "-ggdb");
    } else {
      cmd_append(cmd, "-O3");
    }
    nob_cc_inputs(cmd, input_paths[0]);
    if (!cmd_run(cmd)) return_defer(false);
  }
  nob_log(INFO, "Example '%s' is up to date", example_name);

defer:
  return result;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *program_name = shift(argv, argc);
  while (argc > 0) {
    const char *arg = shift(argv, argc);

    if (streq(arg, "run")) {
      opt.flags.run = true;
      continue;
    }

    if (streq(arg, "build")) {
      opt.flags.force_build = true;
      continue;
    }

    if (streq(arg, "-g")) {
      opt.flags.debug_info = true;
      continue;
    }

    if (streq(arg, "-ts")) {
      opt.flags.ts_build = true;
      continue;
    }

    if (streq(arg, "-ex")) {
      if (argc == 0) {
        nob_log(ERROR, "Missing example name after flag: %s <example-name>", arg);
        return 1;
      }
      const char *example = shift(argv, argc);
      da_append(&opt.examples, example);
      continue;
    }

    if (streq(arg, "-all-ex")) {
      opt.examples.build_all = true;
      continue;
    }
    
    nob_log(ERROR, "Unknown argument provided to build system: %s", arg);
    usage(program_name);
    return 1;
  }

  Cmd cmd = {0};
  const char *output_path;
  const char *input_paths[5];
  size_t input_paths_count = 0;
  input_paths[input_paths_count++] = "./dieq.h";
  if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;


  input_paths_count = 1;
  output_path = BUILD_FOLDER"/dieq-alloc.wasm";
  input_paths[input_paths_count++] = "./wasm.c";
  if (opt.flags.force_build || needs_rebuild(output_path, input_paths, input_paths_count)) {
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
    if (opt.flags.debug_info) {
      cmd_append(&cmd, "-g"); // Embed DWARF debug info
      cmd_append(&cmd, "-O0", "-fno-inline"); // Make sure generated code and source code map 1 to 1
      cmd_append(&cmd, "-gcolumn-info"); // Better breakpoints
      cmd_append(&cmd, "-fno-omit-frame-pointer"); // Full backtraces
      cmd_append(&cmd, "-fsanitize=undefined");
    }
    if (!cmd_run(&cmd)) return 1;

    if (!copy_file(output_path, "dieq-alloc.wasm")) return 1;
  }
  nob_log(INFO, "Output '%s' is up to date", output_path);

  if (opt.flags.ts_build) {
    Nob_File_Paths children = {0};
    if (!opt.flags.force_build) {
      if (!read_entire_dir("./ts-src/", &children)) return 1;
      for (size_t i = 0; i < children.count; ++i) {
        children.items[i] = temp_sprintf("./ts-src/%s", children.items[i]);
      }
    }
    output_path = "main.js";
    if (opt.flags.force_build || needs_rebuild(output_path, children.items, children.count)) {
      // bun build --outfile=main.js ./main.ts
      cmd_append(&cmd, "bun", "build");
      cmd_append(&cmd, "--outfile=main.js", "./ts-src/main.ts");
      if (!cmd_run(&cmd)) return 1;
    }
    da_free(children);
    nob_log(INFO, "Output '%s' is up to date", output_path);
  }

  if (opt.examples.build_all) {
    Nob_File_Paths children = {0};
    if (!read_entire_dir("./examples/", &children)) return 1;
    da_foreach(const char*, it, &children) {
      size_t save_point = temp_save();
      Nob_String_View sv = sv_from_cstr(*it);
      if (!sv_end_with(sv, ".c")) continue;
      sv.count -= 2;
      const char *example_name = temp_sv_to_cstr(sv);
      if (!build_example(&cmd, example_name)) return 1;
      temp_rewind(save_point);
    }
    da_free(children);
  } else if (opt.examples.count > 0) {
    da_foreach(const char*, it, &opt.examples) {
      size_t save_point = temp_save();
      if (!build_example(&cmd, *it)) return 1;
      temp_rewind(save_point);
    }
    if (opt.flags.run && opt.examples.count == 1) {
      opt.flags.run = false;
      cmd_append(&cmd, temp_sprintf(BUILD_FOLDER"/%s", opt.examples.items[0]));
      if (!cmd_run(&cmd)) return 1;
    }
  }

  input_paths_count = 1;
  output_path = BUILD_FOLDER"/dieq-alloc";
  input_paths[input_paths_count++] = "./main.c";
  if (opt.flags.force_build || needs_rebuild(output_path, input_paths, input_paths_count)) {
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, output_path);
    nob_cc_inputs(&cmd, "main.c");

    if (opt.flags.debug_info) {
      cmd_append(&cmd, "-ggdb");
      cmd_append(&cmd, "-fsanitize=address,undefined");
    }
    if (!cmd_run(&cmd)) return 1;
  }
  nob_log(INFO, "Output '%s' is up to date", output_path);

  if (opt.flags.run) {
    cmd_append(&cmd, output_path);
    if (!cmd_run(&cmd)) return 1;
  }

  return 0;
}
