#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "encode.h"


// 各シンボルの数を数える為にポインタを定義
// 数を数える為に、1byteの上限+1で設定しておく
static const int nsymbols = 256 + 1; 
//int symbol_count[nsymbols];
int * symbol_count;

// ノードを表す構造体
typedef struct node
{
  int symbol;
  int count;
  struct node *left;
  struct node *right;
  unsigned int codeword;
  int depth;
  int last_right;
} Node;

// このソースで有効なstatic関数のプロトタイプ宣言
// 一方で、ヘッダファイルは外部からの参照を許す関数の宣言のみ

// ファイルを読み込み、static配列の値を更新する関数
static void count_symbols(const char *filename);

// 与えられた引数でNode構造体を作成し、そのアドレスを返す関数
static Node *create_node(int symbol, int count, Node *left, Node *right);

// Node構造体へのポインタが並んだ配列から、最小カウントを持つ構造体をポップしてくる関数
// n は 配列の実効的な長さを格納する変数を指している（popするたびに更新される）
static Node *pop_min(int *n, Node *nodep[]);

// ハフマン木を構成する関数（根となる構造体へのポインタを返す）
static Node *build_tree(void);

// 木を深さ優先で操作する関数
static void traverse_tree(const int depth, Node *np, unsigned int codeword);

static void print_uint_binary(FILE *fp, unsigned int bit, const int depth);

static void print_tree(Node *np, FILE *fp);

// 以下 static関数の実装
static void count_symbols(const char *filename)
{
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "error: cannot open %s\n", filename);
    exit(1);
  }

  symbol_count = (int*)calloc(nsymbols, sizeof(int));
  
  // 1Byteずつ読み込み、カウントする
 
  unsigned char tmp;
  while (fread(&tmp, sizeof(unsigned char), 1, fp) == 1) {
    symbol_count[(int)tmp]++;
  }

  fclose(fp);
}

static Node *create_node(int symbol, int count, Node *left, Node *right)
{
  Node *ret = (Node *)malloc(sizeof(Node));
  *ret = (Node){ .symbol = symbol, .count = count, .left = left, .right = right};
  return ret;
}

static Node *pop_min(int *n, Node *nodep[])
{
  // Find the node with the smallest count
  // カウントが最小のノードを見つけてくる
  int argmin = 0;
  for (int i = 0; i < *n; i++) {
    if (nodep[i]->count < nodep[argmin]->count) {
      argmin = i;
    }
  }

  Node *node_min = nodep[argmin];

  // Remove the node pointer from nodep[]
  // 見つかったノード以降の配列を前につめていく
  for (int i = argmin; i < (*n) - 1; i++) {
    nodep[i] = nodep[i + 1];
  }
  // 合計ノード数を一つ減らす
  (*n)--;

  return node_min;
}

static Node *build_tree()
{
  int n = 0;
  Node *nodep[nsymbols];

  for (int i = 0; i < nsymbols; i++) {
    // カウントの存在しなかったシンボルには何もしない
    if (symbol_count[i] == 0) continue;

    // 以下は nodep[n++] = create_node(i, symbol_count[i], NULL, NULL); と1行でもよい
    nodep[n] = create_node(i, symbol_count[i], NULL, NULL);
    n++;
  }

  const int dummy = -1; // ダミー用のsymbol を用意しておく
  while (n >= 2) {
    Node *node1 = pop_min(&n, nodep);
    Node *node2 = pop_min(&n, nodep);

    // Create a new node
    // 選ばれた2つのノードを元に統合ノードを新規作成
    // 作成したノードはnodep にどうすればよいか?
    int tmp_cnt = node1->count + node2->count;
    if (node1->symbol != -1 && node2->symbol == -1) {
      nodep[n] = create_node(dummy, tmp_cnt, node1, node2); //　
    }
    else {
      nodep[n] = create_node(dummy, tmp_cnt, node2, node1); //　右に行くほどカウントが小さい
    }
    // printf("cnt1=%d, cnt2=%d\n", node1->count, node2->count);
    n++;
  }
  // 木にした後は symbol_counts は free
  free(symbol_count);
  return (n==0)?NULL:nodep[0];
}

// Perform depth-first traversal of the tree
// 深さ優先で木を走査する
static void traverse_tree(const int depth, Node *np, const unsigned int codeword)
{			  
	
  if (np->left == NULL && np->right == NULL) { //葉まで来た
    // char sym = (unsigned char)(np->symbol);
    np->codeword = codeword;
    np->depth = depth;
    // if (sym == '\n') printf("symbol: \\n, " );
    // else printf("symbol: %c, ", sym);
    
    // printf("count:%d, ", np->count);
    // printf("codeword:");
    // print_uint_binary(codeword, depth); 
    // printf("\n");

    return;
  }

  if (np->left == NULL) return;
  
  unsigned int lcode = (codeword << 1); // 0を左につける
  unsigned int rcode = (codeword << 1) | 1; // 1を左につける
  
  np->left->last_right = np->last_right;
  traverse_tree(depth + 1, np->left, lcode); 
  if (np->right != NULL) {
    np->right->last_right = depth;
    traverse_tree(depth + 1, np->right, rcode);
  }
}

// この関数のみ外部 (main) で使用される (staticがついていない)
int encode(const char *filename, FILE *fp)
{
  count_symbols(filename);
  Node *root = build_tree();
  if (root == NULL){
    fprintf(stderr,"A tree has not been constructed.\n");
    return EXIT_FAILURE;
  }
  traverse_tree(0, root, 0);
  
  fprintf(fp, "Huffman tree has been constructed\n");
  fprintf(fp, ".\n");
  fprintf(fp, "+");
  print_tree(root, fp);
  printf("\n");
  return EXIT_SUCCESS;
}

static void print_tree(Node *np, FILE *fp) {
  
  if (np == NULL) return;

  if (np->left == NULL) { // 再帰の末端の葉
    // fprintf(fp, "|");
    int lasr = np->last_right;
    int d = np->depth;
    char c = np->symbol;

    for (int i = 0; i < 2 * lasr; i++) {
      fprintf(fp, " ");
    }
    if (lasr > 0) fprintf(fp, "|-");
    else fprintf(fp, "--");
    for (int i = 0; i < 2*(d-lasr-1); i++) {
      fprintf(fp, "-");
    }
    
    if (c != '\n') fprintf(fp, "%c:", c);
    else fprintf(fp, "\\n:");
    print_uint_binary(fp, np->codeword, np->depth);
    fprintf(fp, "\n|");
    
    return;
  }

  print_tree(np->left, fp);
  print_tree(np->right, fp);
}

static void print_uint_binary(FILE *fp, unsigned int bit, const int depth) {
    
  unsigned int devisor = 1 << (depth-1);
  while (devisor > 0)
  {
    int b = bit / devisor;
    fprintf(fp, "%d", b);
    bit %= devisor;
    devisor /= 2;
  }
  // printf("\n");
}