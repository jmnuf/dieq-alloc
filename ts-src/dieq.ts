import type { pointer } from './wasm32-helpers';
import { align_up } from './wasm32-helpers';

export type Allocator = {
  readonly memory: WebAssembly.Memory;

  alloc(n: number): pointer;
  free(pointer: pointer): void;
  realloc(p: pointer, n: number): pointer;
  align_up(n: number, a?: number): number;
};

type Result<T, E> = { ok: true; value: T; } | { ok: false; excuse: E; };
type AsyncResult<T, E = unknown> = Promise<Result<T, E>>;
const Result = {
  Ok<T, E>(value: T): Result<T, E> {
    return { ok: true, value };
  },
  Er<T, E>(excuse: E): Result<T, E> {
    return { ok: false, excuse };
  },
} as const;

export async function load_wasm(): AsyncResult<{ allocator: Allocator; instance: WebAssembly.Instance }> {
  try {
    const source = await WebAssembly.instantiateStreaming(fetch('./dieq-alloc.wasm'), { env: {} });
    const memory = source.instance.exports.memory as WebAssembly.Memory;

    // void dieq_global_setup(void *start, void *end);
    const dieq_setup = source.instance.exports.dieq_global_setup as (start: number, end: number) => void;
    // void *dieq_alloc(dieq_uisz size);
    const dieq_alloc = source.instance.exports.dieq_alloc as (size: number) => pointer;
    // void *dieq_realloc(void *ptr, dieq_uisz new_size)
    const dieq_realloc = source.instance.exports.dieq_realloc as (old_ptr: pointer, new_size: number) => pointer;
    // void dieq_free(void *ptr);
    const dieq_free = source.instance.exports.dieq_realloc as (p: pointer) => void;

    const heap_base = (source.instance.exports.__heap_base as WebAssembly.Global).value as number;
    const heap_end = (source.instance.exports.__heap_end as WebAssembly.Global).value as number;

    console.log(source.instance.exports);

    dieq_setup(heap_base, heap_end);

    const allocator: Allocator = {
      alloc: (n) => dieq_alloc(n),
      realloc: (p, n) => dieq_realloc(p, n),
      free(p) {
        if (p == 0) {
          throw new Error('SIGSEV: Trying to free a null pointer');
        }
        dieq_free(p);
      },

      align_up: align_up,

      get memory() {
        return memory;
      },
    };

    return Result.Ok({ allocator, instance: source.instance });
  } catch (excuse) {
    return Result.Er(excuse);
  }
}

