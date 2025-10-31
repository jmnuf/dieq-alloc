import { Struct, type pointer } from './wasm32-helpers.ts';
import { load_wasm } from './dieq.ts';

const IntList = Struct([
  ['head', 'pointer'],
  ['len', 'size_t']
], {
  head() {
    const head_ptr = this.read('head');
    if (head_ptr === 0) return null;
    return new IntNode(this.memory, head_ptr);
  },

  iter: function*() {
    const head_ptr = this.read('head');
    if (head_ptr === 0) return;
    let node: InstanceType<typeof IntNode> | null = new IntNode(this.memory, head_ptr);
    while (node) {
      yield node;
      node = node.next();
    }
  },

  append(value: number): boolean {
    const view = this.view();
    const head_ptr = view.head;
    const ptr = allocator.alloc(IntNode.sizeof);
    if (ptr === 0) return false;
    if (view.head === 0) {
      view.head = ptr;
      const node_ref = new IntNode(this.memory, ptr);
      const node = node_ref.view();
      node.next = 0;
      node.value = value;
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
  },
});

const IntNode = Struct([
  ['next', 'pointer'],
  ['value', 'int'],
], {
  next() {
    const next_ptr = this.read('next');
    if (next_ptr === 0) return null;
    return new IntNode(this.memory, next_ptr);
  },
});

const ListList = Struct([
  ['head', 'pointer'],
  ['len', 'size_t']
], {
  head() {
    const head_ptr = this.read('head');
    if (head_ptr === 0) return null;
    return new IntNode(this.memory, head_ptr);
  },

  iter: function*() {
    const head_ptr = this.read('head');
    if (head_ptr === 0) return;
    let node: InstanceType<typeof IntListListNode> | null = new IntListListNode(this.memory, head_ptr);
    while (node) {
      yield node;
      node = node.next();
    }
  },

  append(value: pointer): boolean {
    const view = this.view();
    const head_ptr = view.head;
    const ptr = allocator.alloc(IntListListNode.sizeof);
    if (ptr === 0) return false;
    if (view.head === 0) {
      view.head = ptr;
      const node_ref = new IntListListNode(this.memory, ptr);
      const node = node_ref.view();
      node.next = 0;
      node.value = value;
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
  },
});

const IntListListNode = Struct([
  ['next', 'pointer'],
  ['value', 'pointer'],
], {
  next() {
    const next_ptr = this.read('next');
    if (next_ptr === 0) return null;
    return new IntListListNode(this.memory, next_ptr);
  },
});

console.log('sizeof(Linked_List_Node) =', IntNode.sizeof);
console.log('sizeof(Linked_List) =', IntList.sizeof);
console.log('offsetof(Linked_List, len) =', IntList.offsetof('len'));

const result = await load_wasm();
if (!result.ok) {
  throw result.excuse;
}
const { allocator } = result.value;

const memory = allocator.memory;

const lists = new ListList(memory, allocator.alloc(ListList.sizeof));

const appDiv = document.querySelector<HTMLDivElement>('#app')!;
appDiv.innerHTML = `
<h2>Let's Allocate Some Memory</h2>
<p id="errors"></p>
<button id="new-list-btn" type="button">Create List</button>
<div id="lists-grid"></div>
`;

const listsDiv = appDiv.querySelector<HTMLDivElement>('#lists-grid')!;
let controller = null as AbortController | null;
const render_lists = () => {
  controller?.abort();
  controller = new AbortController();
  listsDiv.innerHTML = '';
  let i = 0;
  for (const l of lists.iter()) {
    const name = 'Listx' + i.toString(16).padStart(2, '0');
    const pList = l.read('value');
    const list = new IntList(l.memory, pList);
    const div = document.createElement('div');
    const subtitle = document.createElement('h3');
    subtitle.innerText = name;
    div.appendChild(subtitle);
    const btn = document.createElement('button');
    btn.innerText = 'Add Item';
    div.appendChild(btn);
    btn.addEventListener('click', () => {
      try {
        const input = window.prompt('Number to add to list ' + name + ' (only integers allowed):', '34')?.trim();
        if (input) {
          let n: number;
          if (input.startsWith('0x')) {
            n = Number.parseInt(input.substring(2), 16);
          } else if (input.startsWith('b')) {
            n = Number.parseInt(input.substring(1), 2);
          } else {
            n = Number.parseInt(input, 10);
          }
          list.append(n);
          render_lists();
        }
      } catch (_) { }
    }, { signal: controller.signal });
    const ol = document.createElement('ol');
    for (const n of list.iter()) {
      const v = n.read('value');
      const li = document.createElement('li');
      li.innerText = '0x' + v.toString(16).padStart(4, '0');
      ol.appendChild(li);
    }
    div.appendChild(ol);
    listsDiv.appendChild(div);
    ++i;
  }
};

const errorsP = appDiv.querySelector<HTMLButtonElement>('#errors')!;

appDiv.querySelector<HTMLButtonElement>('#new-list-btn')!.addEventListener('click', () => {
  const pList = allocator.alloc(IntList.sizeof);
  if (!lists.append(pList)) {
    errorsP.innerHTML = 'Failed to allocate new list: <strong>Out of Memory</strong>';
  } else {
    render_lists();
  }
});

