#include "q_shared.h"
#include "qcommon.h"

static int			bloc = 0;

// ИСПРАВЛЕНИЕ C6262: Переносим огромные буферы из стека в статическую память
// Это предотвращает Stack Overflow в Native DLL
static byte		  huff_seq[65536];
static huff_t	  huff_state;

void Huff_putBit(int bit, byte* fout, int* offset) {
	bloc = *offset;
	if ((bloc & 7) == 0) {
		fout[(bloc >> 3)] = 0;
	}
	fout[(bloc >> 3)] |= bit << (bloc & 7);
	bloc++;
	*offset = bloc;
}

int		Huff_getBloc(void)
{
	return bloc;
}

void	Huff_setBloc(int _bloc)
{
	bloc = _bloc;
}

int		Huff_getBit(byte* fin, int* offset) {
	int t;
	bloc = *offset;
	t = (fin[(bloc >> 3)] >> (bloc & 7)) & 0x1;
	bloc++;
	*offset = bloc;
	return t;
}

static void add_bit(char bit, byte* fout) {
	if ((bloc & 7) == 0) {
		fout[(bloc >> 3)] = 0;
	}
	fout[(bloc >> 3)] |= bit << (bloc & 7);
	bloc++;
}

static int get_bit(byte* fin) {
	int t;
	t = (fin[(bloc >> 3)] >> (bloc & 7)) & 0x1;
	bloc++;
	return t;
}

static node_t** get_ppnode(huff_t* huff) {
	node_t** tppnode;
	if (!huff->freelist) {
		return &(huff->nodePtrs[huff->blocPtrs++]);
	}
	else {
		tppnode = huff->freelist;
		huff->freelist = (node_t**)*tppnode;
		return tppnode;
	}
}

static void free_ppnode(huff_t* huff, node_t** ppnode) {
	*ppnode = (node_t*)huff->freelist;
	huff->freelist = ppnode;
}

static void swap(huff_t* huff, node_t* node1, node_t* node2) {
	node_t* par1, * par2;

	if (!node1 || !node2) return;

	par1 = node1->parent;
	par2 = node2->parent;

	if (par1) {
		if (par1->left == node1) {
			par1->left = node2;
		}
		else {
			par1->right = node2;
		}
	}
	else {
		huff->tree = node2;
	}

	if (par2) {
		if (par2->left == node2) {
			par2->left = node1;
		}
		else {
			par2->right = node1;
		}
	}
	else {
		huff->tree = node1;
	}

	node1->parent = par2;
	node2->parent = par1;
}

/* ИСПРАВЛЕНИЕ C28182: Добавлены проверки на NULL и корректность связей */
static void swaplist(node_t* node1, node_t* node2) {
	node_t* temp;

	if (!node1 || !node2) return;

	temp = node1->next;
	node1->next = node2->next;
	node2->next = temp;

	temp = node1->prev;
	node1->prev = node2->prev;
	node2->prev = temp;

	if (node1->next == node1) node1->next = node2;
	if (node2->next == node2) node2->next = node1;

	if (node1->next) node1->next->prev = node1;
	if (node2->next) node2->next->prev = node2;
	if (node1->prev) node1->prev->next = node1;
	if (node2->prev) node2->prev->next = node2;
}

static void increment(huff_t* huff, node_t* node) {
	node_t* lnode;

	if (!node) return;

	if (node->next != NULL && node->next->weight == node->weight) {
		lnode = *node->head;
		if (lnode != node->parent) {
			swap(huff, lnode, node);
		}
		swaplist(lnode, node);
	}
	if (node->prev && node->prev->weight == node->weight) {
		*node->head = node->prev;
	}
	else {
		if (node->head) {
			*node->head = NULL;
			free_ppnode(huff, node->head);
		}
	}
	node->weight++;
	if (node->next && node->next->weight == node->weight) {
		node->head = node->next->head;
	}
	else {
		node->head = get_ppnode(huff);
		if (node->head) *node->head = node;
	}
	if (node->parent) {
		increment(huff, node->parent);
		if (node->prev == node->parent) {
			swaplist(node, node->parent);
			if (node->head && *node->head == node) {
				*node->head = node->parent;
			}
		}
	}
}

void Huff_addRef(huff_t* huff, byte ch) {
	node_t* tnode, * tnode2;
	if (huff->loc[ch] == NULL) {
		tnode = &(huff->nodeList[huff->blocNode++]);
		tnode2 = &(huff->nodeList[huff->blocNode++]);

		tnode2->symbol = INTERNAL_NODE;
		tnode2->weight = 1;
		tnode2->next = huff->lhead->next;
		if (huff->lhead->next) {
			huff->lhead->next->prev = tnode2;
			if (huff->lhead->next->weight == 1) {
				tnode2->head = huff->lhead->next->head;
			}
			else {
				tnode2->head = get_ppnode(huff);
				if (tnode2->head) *tnode2->head = tnode2;
			}
		}
		else {
			tnode2->head = get_ppnode(huff);
			if (tnode2->head) *tnode2->head = tnode2;
		}
		huff->lhead->next = tnode2;
		tnode2->prev = huff->lhead;

		tnode->symbol = ch;
		tnode->weight = 1;
		tnode->next = huff->lhead->next;
		if (huff->lhead->next) {
			huff->lhead->next->prev = tnode;
			if (huff->lhead->next->weight == 1) {
				tnode->head = huff->lhead->next->head;
			}
			else {
				tnode->head = get_ppnode(huff);
				if (tnode->head) *tnode->head = tnode2;
			}
		}
		else {
			tnode->head = get_ppnode(huff);
			if (tnode->head) *tnode->head = tnode;
		}
		huff->lhead->next = tnode;
		tnode->prev = huff->lhead;
		tnode->left = tnode->right = NULL;

		if (huff->lhead->parent) {
			if (huff->lhead->parent->left == huff->lhead) {
				huff->lhead->parent->left = tnode2;
			}
			else {
				huff->lhead->parent->right = tnode2;
			}
		}
		else {
			huff->tree = tnode2;
		}

		tnode2->right = tnode;
		tnode2->left = huff->lhead;

		tnode2->parent = huff->lhead->parent;
		huff->lhead->parent = tnode->parent = tnode2;

		huff->loc[ch] = tnode;

		increment(huff, tnode2->parent);
	}
	else {
		increment(huff, huff->loc[ch]);
	}
}

int Huff_Receive(node_t* node, int* ch, byte* fin) {
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin)) {
			node = node->right;
		}
		else {
			node = node->left;
		}
	}
	if (!node) {
		return 0;
	}
	return (*ch = node->symbol);
}

void Huff_offsetReceive(node_t* node, int* ch, byte* fin, int* offset) {
	bloc = *offset;
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin)) {
			node = node->right;
		}
		else {
			node = node->left;
		}
	}
	if (!node) {
		*ch = 0;
		return;
	}
	*ch = node->symbol;
	*offset = bloc;
}

static void send(node_t* node, node_t* child, byte* fout) {
	if (node->parent) {
		send(node->parent, node, fout);
	}
	if (child) {
		if (node->right == child) {
			add_bit(1, fout);
		}
		else {
			add_bit(0, fout);
		}
	}
}

void Huff_transmit(huff_t* huff, int ch, byte* fout) {
	int i;
	if (huff->loc[ch] == NULL) {
		Huff_transmit(huff, NYT, fout);
		for (i = 7; i >= 0; i--) {
			add_bit((char)((ch >> i) & 0x1), fout);
		}
	}
	else {
		send(huff->loc[ch], NULL, fout);
	}
}

void Huff_offsetTransmit(huff_t* huff, int ch, byte* fout, int* offset) {
	bloc = *offset;
	send(huff->loc[ch], NULL, fout);
	*offset = bloc;
}

/* ИСПРАВЛЕНИЕ: Используем huff_state и huff_seq вместо стека */
void Huff_Decompress(msg_t* mbuf, int offset) {
	int			ch, cch, i, j, size;
	byte* buffer;

	size = mbuf->cursize - offset;
	buffer = mbuf->data + offset;

	if (size <= 0) return;

	Com_Memset(&huff_state, 0, sizeof(huff_t));
	huff_state.tree = huff_state.lhead = huff_state.ltail = huff_state.loc[NYT] = &(huff_state.nodeList[huff_state.blocNode++]);
	huff_state.tree->symbol = NYT;
	huff_state.tree->weight = 0;
	huff_state.lhead->next = huff_state.lhead->prev = NULL;
	huff_state.tree->parent = huff_state.tree->left = huff_state.tree->right = NULL;

	cch = buffer[0] * 256 + buffer[1];
	if (cch > mbuf->maxsize - offset) cch = mbuf->maxsize - offset;
	if (cch > 65536) cch = 65536; // Защита буфера

	bloc = 16;

	for (j = 0; j < cch; j++) {
		ch = 0;
		if ((bloc >> 3) > size) {
			huff_seq[j] = 0;
			break;
		}
		Huff_Receive(huff_state.tree, &ch, buffer);
		if (ch == NYT) {
			ch = 0;
			for (i = 0; i < 8; i++) {
				ch = (ch << 1) + get_bit(buffer);
			}
		}
		huff_seq[j] = (byte)ch;
		Huff_addRef(&huff_state, (byte)ch);
	}
	mbuf->cursize = cch + offset;
	Com_Memcpy(mbuf->data + offset, huff_seq, cch);
}

void Huff_Compress(msg_t* mbuf, int offset) {
	int			i, ch, size;
	byte* buffer;

	size = mbuf->cursize - offset;
	buffer = mbuf->data + offset;

	if (size <= 0) return;

	Com_Memset(&huff_state, 0, sizeof(huff_t));
	huff_state.tree = huff_state.lhead = huff_state.loc[NYT] = &(huff_state.nodeList[huff_state.blocNode++]);
	huff_state.tree->symbol = NYT;
	huff_state.tree->weight = 0;
	huff_state.lhead->next = huff_state.lhead->prev = NULL;
	huff_state.tree->parent = huff_state.tree->left = huff_state.tree->right = NULL;

	huff_seq[0] = (byte)(size >> 8);
	huff_seq[1] = (byte)(size & 0xff);

	bloc = 16;

	for (i = 0; i < size; i++) {
		ch = buffer[i];
		Huff_transmit(&huff_state, ch, huff_seq);
		Huff_addRef(&huff_state, (byte)ch);
	}

	bloc += 8;
	mbuf->cursize = (bloc >> 3) + offset;
	Com_Memcpy(mbuf->data + offset, huff_seq, (bloc >> 3));
}

void Huff_Init(huffman_t* huff) {
	Com_Memset(&huff->compressor, 0, sizeof(huff_t));
	Com_Memset(&huff->decompressor, 0, sizeof(huff_t));

	huff->decompressor.tree = huff->decompressor.lhead = huff->decompressor.ltail = huff->decompressor.loc[NYT] = &(huff->decompressor.nodeList[huff->decompressor.blocNode++]);
	huff->decompressor.tree->symbol = NYT;
	huff->decompressor.tree->weight = 0;
	huff->decompressor.lhead->next = huff->decompressor.lhead->prev = NULL;
	huff->decompressor.tree->parent = huff->decompressor.tree->left = huff->decompressor.tree->right = NULL;

	huff->compressor.tree = huff->compressor.lhead = huff->compressor.loc[NYT] = &(huff->compressor.nodeList[huff->compressor.blocNode++]);
	huff->compressor.tree->symbol = NYT;
	huff->compressor.tree->weight = 0;
	huff->compressor.lhead->next = huff->compressor.lhead->prev = NULL;
	huff->compressor.tree->parent = huff->compressor.tree->left = huff->compressor.tree->right = NULL;
}