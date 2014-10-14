
 
typedef struct node_t {
	struct node_t *left, *right;
	int freq;
	char c;
} *node;
 
struct node_t pool[256] = {{0}};
node qqq[255], *q = qqq - 1;
int n_nodes = 0, qend = 1;
char *code[128] = {0}, buf[1024];
 
node new_node(int freq, char c, node a, node b)
{
	node n = pool + n_nodes++;
	if (freq) n->c = c, n->freq = freq;
	else {
		n->left = a, n->right = b;
		n->freq = a->freq + b->freq;
	}
	return n;
}
 
/* priority queue */
void qinsert(node n)
{
	int j, i = qend++;
	while ((j = i / 2)) {
		if (q[j]->freq <= n->freq) break;
		q[i] = q[j], i = j;
	}
	q[i] = n;
}

node qremove()
{
	int i, l;
	node n = q[i = 1];
 
	if (qend < 2) return 0;
	qend--;
	while ((l = i * 2) < qend) {
		if (l + 1 < qend && q[l + 1]->freq < q[l]->freq) l++;
		q[i] = q[l], i = l;
	}
	q[i] = q[qend];
	return n;
}
 
