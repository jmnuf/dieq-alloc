// ts-src/wasm32-helpers.ts
function align_up(n, alignment = 4) {
  if ((alignment & alignment - 1) != 0) {
    throw new Error("Alignment must be a power of 2 value");
  }
  return n + (n & alignment - 1);
}
function sizeof(k) {
  if (Array.isArray(k)) {
    let size = 0;
    for (let i = 0;i < k.length; ++i) {
      if (i > 0)
        size = align_up(size, sizeof(k[i][1]));
      size += sizeof(k[i][1]);
    }
    return size;
  }
  switch (k) {
    case "bool":
    case "char":
      return 1;
    case "int":
    case "float":
    case "size_t":
    case "pointer":
      return 4;
    case "unsigned long long":
    case "long long":
    case "double":
      return 8;
  }
  throw new Error(`Unknown kind: ${k}`);
}
function offsetof(Struct, property) {
  let offset = 0;
  for (const [field_name, field_type] of Struct) {
    const s = sizeof(field_type);
    if (offset > 0)
      offset = align_up(offset, s);
    if (property == field_name)
      return offset;
    offset += s;
  }
  return -1;
}
function Struct(fields, methods = {}) {
  const size = sizeof(fields);
  const offsets = fields.map(([fname, ftype]) => {
    return [fname, offsetof(fields, fname), ftype];
  });
  const MyStruct = class {
    ptr;
    memory;
    constructor(memory, ptr) {
      this.ptr = ptr;
      this.memory = memory;
    }
    read(field_name) {
      const field = offsets.find(([fname]) => field_name === fname);
      if (!field) {
        throw new Error("Unknown field: " + String(field_name));
      }
      const [_, foff, ftype] = field;
      const ptr = this.ptr + foff;
      const v = new DataView(this.memory.buffer, ptr);
      let val = undefined;
      switch (ftype) {
        case "bool":
          val = v.getUint8(0) == 1;
          break;
        case "char":
          val = v.getInt8(0);
          break;
        case "int":
          val = v.getInt32(0, true);
          break;
        case "size_t":
        case "pointer":
          val = v.getUint32(0, true);
          break;
        case "long long":
          val = v.getBigInt64(0, true);
          break;
        case "unsigned long long":
          val = v.getBigUint64(0, true);
          break;
        case "float":
          val = v.getFloat32(0, true);
          break;
        case "double":
          val = v.getFloat64(0, true);
          break;
        default:
          const _never = ftype;
          break;
      }
      return val;
    }
    view() {
      const buf = this.memory.buffer;
      const obj = {};
      const v = new DataView(buf, this.ptr);
      for (const [fname, off, ftype] of offsets) {
        let reader = () => {
          throw new Error(`No reader for ${fname}: ${ftype}`);
        };
        let writer = () => {
          throw new Error(`No writer for ${fname}: ${ftype}`);
        };
        switch (ftype) {
          case "bool":
            reader = () => v.getUint8(off) == 1;
            writer = (value) => {
              if (typeof value != "boolean") {
                throw new TypeError(`Property ${fname} must be of type boolean`);
              }
              v.setUint8(off, +value);
            };
            break;
          case "char":
            reader = () => v.getInt8(off);
            writer = (value) => {
              if (typeof value != "number") {
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
          case "int":
            reader = () => v.getInt32(off, true);
            writer = (value) => {
              if (typeof value != "number") {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setInt32(off, value, true);
            };
            break;
          case "size_t":
          case "pointer":
            reader = () => v.getUint32(off, true);
            writer = (value) => {
              if (typeof value != "number") {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setUint32(off, value, true);
            };
            break;
          case "long long":
            reader = () => v.getBigInt64(off, true);
            writer = (value) => {
              if (typeof value != "bigint") {
                throw new TypeError(`Property ${fname} must be of type bigint`);
              }
              v.setBigInt64(off, value, true);
            };
            break;
          case "unsigned long long":
            reader = () => v.getBigUint64(off, true);
            writer = (value) => {
              if (typeof value != "bigint") {
                throw new TypeError(`Property ${fname} must be of type bigint`);
              }
              v.setBigUint64(off, value, true);
            };
            break;
          case "float":
            reader = () => v.getFloat32(off, true);
            writer = (value) => {
              if (typeof value != "number") {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setFloat32(off, value, true);
            };
            break;
          case "double":
            reader = () => v.getFloat64(off, true);
            writer = (value) => {
              if (typeof value != "number") {
                throw new TypeError(`Property ${fname} must be of type number`);
              }
              if (!Number.isInteger(value)) {
                throw new TypeError(`Property ${fname} must be of an integer`);
              }
              v.setFloat64(off, value, true);
            };
            break;
          default:
            const _never = ftype;
            break;
        }
        Object.defineProperty(obj, fname, {
          configurable: false,
          enumerable: true,
          get: reader,
          set: writer
        });
      }
      return obj;
    }
    static sizeof = 0;
    static offsetof = (field_name) => offsetof(fields, field_name);
  };
  for (const m_name of Object.keys(methods)) {
    MyStruct.prototype[m_name] = methods[m_name];
  }
  Object.defineProperty(MyStruct, "sizeof", {
    configurable: false,
    enumerable: true,
    writable: false,
    value: size
  });
  return MyStruct;
}

// ts-src/dieq.ts
var Result = {
  Ok(value) {
    return { ok: true, value };
  },
  Er(excuse) {
    return { ok: false, excuse };
  }
};
async function load_wasm() {
  try {
    const source = await WebAssembly.instantiateStreaming(fetch("./dieq-alloc.wasm"), { env: {} });
    const memory = source.instance.exports.memory;
    const dieq_setup = source.instance.exports.dieq_global_setup;
    const dieq_alloc = source.instance.exports.dieq_alloc;
    const dieq_realloc = source.instance.exports.dieq_realloc;
    const dieq_free = source.instance.exports.dieq_realloc;
    const heap_base = source.instance.exports.__heap_base.value;
    const heap_end = source.instance.exports.__heap_end.value;
    console.log(source.instance.exports);
    dieq_setup(heap_base, heap_end);
    const allocator = {
      alloc: (n) => dieq_alloc(n),
      realloc: (p, n) => dieq_realloc(p, n),
      free(p) {
        if (p == 0) {
          throw new Error("SIGSEV: Trying to free a null pointer");
        }
        dieq_free(p);
      },
      align_up,
      get memory() {
        return memory;
      }
    };
    return Result.Ok({ allocator, instance: source.instance });
  } catch (excuse) {
    return Result.Er(excuse);
  }
}

// ts-src/main.ts
var IntList = Struct([
  ["head", "pointer"],
  ["len", "size_t"]
], {
  head() {
    const head_ptr = this.read("head");
    if (head_ptr === 0)
      return null;
    return new IntNode(this.memory, head_ptr);
  },
  iter: function* () {
    const head_ptr = this.read("head");
    if (head_ptr === 0)
      return;
    let node = new IntNode(this.memory, head_ptr);
    while (node) {
      yield node;
      node = node.next();
    }
  },
  append(value) {
    const view = this.view();
    const head_ptr = view.head;
    const ptr = allocator.alloc(IntNode.sizeof);
    if (ptr === 0)
      return false;
    if (view.head === 0) {
      view.head = ptr;
      const node_ref = new IntNode(this.memory, ptr);
      const node2 = node_ref.view();
      node2.next = 0;
      node2.value = value;
      view.len = 1;
      return true;
    }
    let node = new IntNode(this.memory, head_ptr);
    let next = node.next();
    while (next) {
      node = next;
      next = node.next();
    }
    node.view().next = ptr;
    node = new IntNode(this.memory, ptr);
    const nv = node.view();
    nv.value = value;
    nv.next = 0;
    view.len++;
    return true;
  }
});
var IntNode = Struct([
  ["next", "pointer"],
  ["value", "int"]
], {
  next() {
    const next_ptr = this.read("next");
    if (next_ptr === 0)
      return null;
    return new IntNode(this.memory, next_ptr);
  }
});
var ListList = Struct([
  ["head", "pointer"],
  ["len", "size_t"]
], {
  head() {
    const head_ptr = this.read("head");
    if (head_ptr === 0)
      return null;
    return new IntNode(this.memory, head_ptr);
  },
  iter: function* () {
    const head_ptr = this.read("head");
    if (head_ptr === 0)
      return;
    let node = new IntListListNode(this.memory, head_ptr);
    while (node) {
      yield node;
      node = node.next();
    }
  },
  append(value) {
    const view = this.view();
    const head_ptr = view.head;
    const ptr = allocator.alloc(IntListListNode.sizeof);
    if (ptr === 0)
      return false;
    if (view.head === 0) {
      view.head = ptr;
      const node_ref = new IntListListNode(this.memory, ptr);
      const node2 = node_ref.view();
      node2.next = 0;
      node2.value = value;
      view.len = 1;
      return true;
    }
    let node = new IntListListNode(this.memory, head_ptr);
    let next = node.next();
    while (next) {
      node = next;
      next = node.next();
    }
    node.view().next = ptr;
    node = new IntListListNode(this.memory, ptr);
    const nv = node.view();
    nv.value = value;
    nv.next = 0;
    view.len++;
    return true;
  }
});
var IntListListNode = Struct([
  ["next", "pointer"],
  ["value", "pointer"]
], {
  next() {
    const next_ptr = this.read("next");
    if (next_ptr === 0)
      return null;
    return new IntListListNode(this.memory, next_ptr);
  }
});
console.log("sizeof(Linked_List_Node) =", IntNode.sizeof);
console.log("sizeof(Linked_List) =", IntList.sizeof);
console.log("offsetof(Linked_List, len) =", IntList.offsetof("len"));
var result = await load_wasm();
if (!result.ok) {
  throw result.excuse;
}
var { allocator } = result.value;
var memory = allocator.memory;
var lists = new ListList(memory, allocator.alloc(ListList.sizeof));
var appDiv = document.querySelector("#app");
appDiv.innerHTML = `
<h2>Let's Allocate Some Memory</h2>
<p id="errors"></p>
<button id="new-list-btn" type="button">Create List</button>
<div id="lists-grid"></div>
`;
var listsDiv = appDiv.querySelector("#lists-grid");
var controller = null;
var render_lists = () => {
  controller?.abort();
  controller = new AbortController;
  listsDiv.innerHTML = "";
  let i = 0;
  for (const l of lists.iter()) {
    const name = "Listx" + i.toString(16).padStart(2, "0");
    const pList = l.read("value");
    const list = new IntList(l.memory, pList);
    const div = document.createElement("div");
    const subtitle = document.createElement("h3");
    subtitle.innerText = name;
    div.appendChild(subtitle);
    const btn = document.createElement("button");
    btn.innerText = "Add Item";
    div.appendChild(btn);
    btn.addEventListener("click", () => {
      try {
        const input = window.prompt("Number to add to list " + name + " (only integers allowed):", "34")?.trim();
        if (input) {
          let n;
          if (input.startsWith("0x")) {
            n = Number.parseInt(input.substring(2), 16);
          } else if (input.startsWith("b")) {
            n = Number.parseInt(input.substring(1), 2);
          } else {
            n = Number.parseInt(input, 10);
          }
          list.append(n);
          render_lists();
        }
      } catch (_) {}
    }, { signal: controller.signal });
    const ol = document.createElement("ol");
    for (const n of list.iter()) {
      const v = n.read("value");
      const li = document.createElement("li");
      li.innerText = "0x" + v.toString(16).padStart(4, "0");
      ol.appendChild(li);
    }
    div.appendChild(ol);
    listsDiv.appendChild(div);
    ++i;
  }
};
var errorsP = appDiv.querySelector("#errors");
appDiv.querySelector("#new-list-btn").addEventListener("click", () => {
  const pList = allocator.alloc(IntList.sizeof);
  if (!lists.append(pList)) {
    errorsP.innerHTML = "Failed to allocate new list: <strong>Out of Memory</strong>";
  } else {
    render_lists();
  }
});
