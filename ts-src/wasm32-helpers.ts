type Prettify<T> = {
  [K in keyof T]: T[K];
} & unknown;

type Lower_ASCII_Alpha_Char = 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i' | 'j' | 'k' | 'l' | 'm' | 'n' | 'o' | 'p' | 'q' | 'r' | 's' | 't' | 'u' | 'v' | 'w' | 'x' | 'y' | 'z';
type Upper_ASCII_Alpha_Char = Uppercase<Lower_ASCII_Alpha_Char>;
type ASCII_Alpha_Char = Lower_ASCII_Alpha_Char | Upper_ASCII_Alpha_Char;

type Ident_Start_Char = ASCII_Alpha_Char | '_';

export type pointer = number & {};

interface Wasm32TypeMap {
  'bool': boolean;
  'char': number;
  'int': number;
  'float': number;
  'size_t': number;
  'pointer': pointer;
  'double': number;
  'unsigned long long': bigint;
  'long long': bigint;
}

export type Wasm32Type = keyof Wasm32TypeMap;
type Wasm32Type_2_JS_Type<T extends Wasm32Type> = Wasm32TypeMap[T];

export type Struct_Shape = Array<[`${Ident_Start_Char}${string}`, Wasm32Type]>;
type Struct_Methods<S extends Struct_Shape> = Record<string | symbol, (this: IStruct<S>, ...args: any[]) => any>;

type Struct_View<S extends Struct_Shape, Data extends Record<string, any> = {}> =
  S extends []
  ? Prettify<Data>
  : S extends [infer Head extends Struct_Shape[number], ...infer Tail extends Struct_Shape]
  ? Struct_View<Tail, Data & { [K in Head[0]]: Wasm32Type_2_JS_Type<Head[1]> }>
  : Prettify<Data>
  ;

type FindFTypeByFName<S extends Struct_Shape, Name extends Struct_Shape[number][0]> =
  S extends []
  ? never
  : S extends [infer Head extends Struct_Shape[number], ...infer Tail extends Struct_Shape]
  ? Name extends Head[0]
  ? Head[1]
  : FindFTypeByFName<Tail, Name>
  : never
  ;

export interface IStruct<Shape extends Struct_Shape> {
  readonly ptr: pointer;
  readonly memory: WebAssembly.Memory;

  read<Name extends Shape[number][0]>(field_name: Name): Wasm32Type_2_JS_Type<FindFTypeByFName<Shape, Name>>;
  view(): Struct_View<Shape, {}>
}

export interface StructConstructor<Shape extends Struct_Shape, Methods extends Struct_Methods<Shape>> {
  new(memory: WebAssembly.Memory, ptr: pointer): Prettify<IStruct<Shape> & Omit<Methods, keyof IStruct<any>>>;
  readonly sizeof: number;
  offsetof<K extends Shape[number][0]>(field: K): number;
}

export function align_up(n: number, alignment: number = 4) {
  if ((alignment & (alignment - 1)) != 0) {
    throw new Error('Alignment must be a power of 2 value');
  }
  return n + (n & (alignment - 1));
}


export function sizeof(k: Wasm32Type | Struct_Shape) {
  if (Array.isArray(k)) {
    let size = 0;
    for (let i = 0; i < k.length; ++i) {
      if (i > 0) size = align_up(size, sizeof(k[i][1]));
      size += sizeof(k[i][1]);
    }
    return size;
  }

  switch (k) {
    case 'bool':
    case 'char':
      return 1;

    case 'int':
    case 'float':
    case 'size_t':
    case 'pointer':
      return 4;

    case 'unsigned long long':
    case 'long long':
    case 'double':
      return 8;
  }

  throw new Error(`Unknown kind: ${k}`);
}

export function offsetof<S extends Struct_Shape>(Struct: S, property: S[number][0]) {
  let offset = 0;
  for (const [field_name, field_type] of Struct) {
    const s = sizeof(field_type);
    if (offset > 0) offset = align_up(offset, s);
    if (property == field_name) return offset;
    offset += s;
  }
  return -1;
}

export function Struct<const S extends Struct_Shape, const M extends Struct_Methods<S>>(fields: S, methods: M): StructConstructor<S, M>;
export function Struct<const S extends Struct_Shape>(fields: S): StructConstructor<S, {}>;
export function Struct(fields: Struct_Shape, methods: Struct_Methods<any> = {}): any {
  const size = sizeof(fields);
  const offsets = fields.map(([fname, ftype]) => {
    return [fname, offsetof(fields, fname), ftype] as const;
  });
  const MyStruct = class implements IStruct<any> {
    readonly ptr;
    readonly memory;
    constructor(memory: WebAssembly.Memory, ptr: pointer) {
      this.ptr = ptr;
      this.memory = memory;
    }

    read(field_name: Struct_Shape[number][0]): any {
      const field = offsets.find(([fname]) => field_name === fname);
      if (!field) {
        throw new Error('Unknown field: ' + String(field_name));
      }
      const [_, foff, ftype] = field;
      const ptr: pointer = this.ptr + foff;
      const v = new DataView(this.memory.buffer, ptr);
      let val: any = undefined;
      switch (ftype) {
        case 'bool':
          val = v.getUint8(0) == 1;
          break;

        case 'char':
          val = v.getInt8(0);
          break;

        case 'int':
          val = v.getInt32(0, true);
          break;

        case 'size_t':
        case 'pointer':
          val = v.getUint32(0, true);
          break;

        case 'long long':
          val = v.getBigInt64(0, true);
          break;

        case 'unsigned long long':
          val = v.getBigUint64(0, true);
          break;

        case 'float':
          val = v.getFloat32(0, true);
          break;

        case 'double':
          val = v.getFloat64(0, true);
          break;

        default:
          const _never: never = ftype;
          _never;
          break;
      }

      return val;
    }

    view(): Struct_View<any> {
      const buf = this.memory.buffer;
      const obj: any = {};
      const v = new DataView(buf, this.ptr);
      for (const [fname, off, ftype] of offsets) {
        let reader: () => unknown = () => {
          throw new Error(`No reader for ${fname}: ${ftype}`);
        };
        let writer: (v: any) => void = () => {
          throw new Error(`No writer for ${fname}: ${ftype}`);
        };

        switch (ftype) {
          case 'bool':
            reader = () => v.getUint8(off) == 1;
            writer = (value) => {
              if (typeof value != 'boolean') {
                throw new TypeError(`Property ${fname} must be of type boolean`);
              }
              v.setUint8(off, +value);
            };
            break;

          case 'char':
            reader = () => v.getInt8(off);
            writer = (value) => {
              if (typeof value != 'number') {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              value = value >= 255 ? 255 : +value;
              value = value <= 0 ? 0 : value;
              v.setInt8(off, value);
            };
            break;

          case 'int':
            reader = () => v.getInt32(off, true);
            writer = (value) => {
              if (typeof value != 'number') {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setInt32(off, value, true);
            };
            break;

          case 'size_t':
          case 'pointer':
            reader = () => v.getUint32(off, true);
            writer = (value) => {
              if (typeof value != 'number') {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setUint32(off, value, true);
            };
            break;

          case 'long long':
            reader = () => v.getBigInt64(off, true);
            writer = (value) => {
              if (typeof value != 'bigint') {
                throw new TypeError(`Property ${fname} must be of type bigint`);
              }
              v.setBigInt64(off, value, true);
            };
            break;

          case 'unsigned long long':
            reader = () => v.getBigUint64(off, true);
            writer = (value) => {
              if (typeof value != 'bigint') {
                throw new TypeError(`Property ${fname} must be of type bigint`);
              }
              v.setBigUint64(off, value, true);
            };
            break;

          case 'float':
            reader = () => v.getFloat32(off, true);
            writer = (value) => {
              if (typeof value != 'number') {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setFloat32(off, value, true);
            };
            break;

          case 'double':
            reader = () => v.getFloat64(off, true);
            writer = (value) => {
              if (typeof value != 'number') {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setFloat64(off, value, true);
            };
            break;

          default:
            const _never: never = ftype;
            _never;
            break;
        }

        Object.defineProperty(obj, fname, {
          configurable: false,
          enumerable: true,
          get: reader,
          set: writer,
        });
      }
      return obj;
    }

    static sizeof = 0;
    static offsetof = (field_name: Struct_Shape[number][0]) => offsetof(fields, field_name);
  };

  for (const m_name of Object.keys(methods)) {
    // @ts-expect-error Prototype doesn't hold m_name
    MyStruct.prototype[m_name] = methods[m_name];
  }

  Object.defineProperty(MyStruct, 'sizeof', {
    configurable: false,
    enumerable: true,
    writable: false,
    value: size,
  });

  return MyStruct as any;
}

