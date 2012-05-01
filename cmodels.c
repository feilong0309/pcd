#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "cmodels.h"
#include "SFMT.h"

#define DEBUGLISTS (0)
#ifndef DEBUG
#define DEBUG (0)
#endif

int test(Graph_t G) {
  /* Print some test debugging information
   */
  printf("nNodes: %d\n", G->N);
  int i;
  // Print the communities
  printf("cmtys: ");
  for (i=0 ; i<G->N ; i++) {
    printf("%d ", G->cmty[i]);
    G->cmty[i] += 10;
  }
  printf("\n");

  // Print the community lists
  for (i=0 ; i<G->N ; i++) {
    //printf("cmtyi: %x %x %x\n", G->cmtyii, G->cmtyi, G->cmtyi[i]);
    //    printf("cmtyi: %x\n", G->cmtyii);

    //printf("cmty %d: %d %d\n", i, G->cmtyi[i][0], G->cmtyi[i][1]);
  }

  // Print the interaction maxtrix
  for (i=0 ; i<G->N ; i++) {
    int j;
    printf("  ");
    for (j=0 ; j<G->N ; j++) {
#ifdef IMATRIX_T_INT
      printf("%2d ", G->imatrix[i*G->N+j]);
#else
      printf("%5.2f ", G->imatrix[i*G->N+j]);
#endif
    }
    printf("\n");
  }

  return (0);
}

void gThreadInit() {
  //assert(g_thread_supported());
  //assert(!g_thread_get_initialized());
  g_thread_init(NULL);
  //assert(g_thread_get_initialized());
}

int isInCmty(Graph_t G, int c, int n) {
  /* assert(G->oneToOne); */
  /* return (G->cmty[n] == c); */
  if (G->oneToOne)
    return (G->cmty[n] == c);
  else {
    int in_cmty1 = g_hash_table_lookup_extended(G->cmtyListHash[c],
    					       GINT_TO_POINTER(n),
    					       //&n,
    					       NULL, NULL);
    return(in_cmty1);
  }
}


/*
 * Functions which deal with the community lists:
 * - G->cmty[n]   (direct community mapping)
 * - G->cmtyl[c][i]
 * - G->cmtyN[c]
 * - G->Ncmty
 */
inline void cmtyListAddOverlap(Graph_t G, int c, int n) {
  /* Add particle n to community c
   *
   * Adapted for systems that have overlaps.
   */
  if (DEBUG) assert(!isInCmty(G, c, n));
  G->cmtyN[c]++;
  if (DEBUGLISTS)
    printf("cLAO: %2d %2d %2d\n", c, n, G->cmty[n]);
  g_hash_table_insert(G->cmtyListHash[c], GINT_TO_POINTER(n), NULL);
  /*G->cmty[n] = c;*/
  if (c >= G->Ncmty)
    G->Ncmty = c+1;
}
inline void cmtyListAdd(Graph_t G, int c, int n) {
  /* Add particle n to community c
   */
  cmtyListAddOverlap(G, c, n);
  // The difference for systems without overlaps is we keep G->cmty[n]
  // up to date
  G->cmty[n] = c;
}
inline void _cmtyListRemoveOverlap(Graph_t G, int c, int n);
inline void cmtyListRemoveOverlap(Graph_t G, int c, int n) {
  /* Remove particle n from community c
   *
   * Adapted for systems that have overlaps
   */
  if (DEBUG) assert (isInCmty(G, c, n));
  if (DEBUG) {
    int found = g_hash_table_lookup_extended(G->cmtyListHash[c],
					     GINT_TO_POINTER(n),
					     NULL, NULL);
    assert(found);
  }
  _cmtyListRemoveOverlap(G, c, n);
}
inline void _cmtyListRemoveOverlap(Graph_t G, int c, int n) {
  if (DEBUGLISTS)
    printf("cLRO: %2d %2d %2d\n", c, n, G->cmty[n]);

  g_hash_table_remove(G->cmtyListHash[c], GINT_TO_POINTER(n));
  G->cmtyN[c]-- ;
  // If we just removed the greatest-numbered community
  if (c == G->Ncmty-1  &&  G->cmtyN[c] == 0 ) {
    // Altar Ncmty too if we just removed the greatest-numbered community
    int i;
    for (i=G->Ncmty-1 ; i>=0 ; i--) {
      if (G->cmtyN[i] == 0)
	G->Ncmty--;
      else
	break;
    }
  }
}
inline void cmtyListRemove(Graph_t G, int c, int n) {
  /* Remove particle n from community c
   */
  cmtyListRemoveOverlap(G, c, n);
  // The difference for systems without overlaps is we keep G->cmty[n]
  // up to date
  G->cmty[n] = NO_CMTY;
}
inline void cmtyMove(Graph_t G, int n, int cOld, int cNew) {
  /* Move node n from cOld to cNew.
   * This function assumes: the * particle's primary community is
   * cOld, and it is not already in * cNew.
   */
  if (DEBUG) assert (G->cmty[n] == cOld);
  // cOld = G->cmty[n];
  cmtyListRemoveOverlap(G, cOld, n);
  cmtyListAddOverlap(G, cNew, n);
  G->cmty[n] = cNew;
}
inline void cmtyMoveSafe(Graph_t G, int n, int cOld, int cNew) {
  /* Move node n from cOld to cNew.

   * Differes from cmtyMove in that this function can handle several
   * corner c
   */
  //if (DEBUG) assert (isInCmty(G, cOld, n));
  //if (DEBUG) assert (!isInCmty(G, cNew, n));
  //if (isInCmty(G, cOld, n))
  cmtyListRemoveOverlap(G, cOld, n);
  if (! isInCmty(G, cNew, n))
    cmtyListAddOverlap(G, cNew, n);
  G->cmty[n] = cNew;
}
inline void cmtyMoveAll(Graph_t G, int cOld, int cNew) {
  /* Move all nodes in cOld to cNew.
   */
  GHashTableIter hashIter;
  void *n_p;
  g_hash_table_iter_init(&hashIter, G->cmtyListHash[cOld]);
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    if (DEBUG) assert (G->cmty[n] == cOld);
    if (DEBUG) assert (!isInCmty(G, cNew, n));
    g_hash_table_iter_remove(&hashIter);
    _cmtyListRemoveOverlap(G, cOld, n);

    cmtyListAddOverlap(G, cNew, n);
    G->cmty[n] = cNew;
  }
}
inline void cmtyMoveAllOverlap(Graph_t G, int cOld, int cNew) {
  /* Move all nodes in cOld to cNew.
   */
  GHashTableIter hashIter;
  void *n_p;
  g_hash_table_iter_init(&hashIter, G->cmtyListHash[cOld]);
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    g_hash_table_iter_remove(&hashIter);
    _cmtyListRemoveOverlap(G, cOld, n);

    if (! isInCmty(G, cNew, n))
      cmtyListAddOverlap(G, cNew, n);
    if (G->cmty[n] == cOld)
      G->cmty[n] = cNew;
  }
}

inline void cmtyListInit(Graph_t G) {
  /* Initialize the community lists.
   *
   * Initialize all the G->cmtyl[c][0...i] based on G->cmty[n].
   */
  int c, n;
  // This method is always going to initialize to a oneToOne mapping
  // by definition.
  G->oneToOne = 1;
  // Set all lists lengths to zero
  for (c=0 ; c<G->N ; c++) {
    g_hash_table_remove_all(G->cmtyListHash[c]);
    G->cmtyN[c] = 0;
  }
  // Set Ncmty to zero. cmtyListAdd automatically increments this as needed.
  G->Ncmty = 0;
  // Iterate through particles adding them to community lists
  for (n=0 ; n<G->N ; n++) {
    int c = G->cmty[n];
    G->cmty[n] = NO_CMTY; // Without this line, cmtyListAdd fails errorcheck
    cmtyListAdd(G, c, n);
    if (G->cmty[n] >= G->Ncmty)
      printf("****** community %d is greater than Ncmty=%d\n", G->cmty[n],
	     G->Ncmty);
  }
  // Reset Ncmty to what it should be
  for(c=G->N-1 ; c>=0 ; c--) {
    if (G->cmtyN[c] > 0) {
      G->Ncmty = c+1;
      break;
    }
  }
  //hashInit(G);
}
int cmtyListCheck(Graph_t G) {
  /* Check the community lists for consistency.
   *
   * This function will need revisions once one node can be in
   * multiple groups.
   *
   * Returns the numbers of errors found.
   */
  int errors=0;
  /* int i, j, n, m; */
  int cmty;
  // Check Ncmty is indeed the maximum number of communities.
  for (cmty=0; cmty < G->N; cmty++) {
    if ( G->cmtyN[cmty] > 0   &&  cmty >= G->Ncmty ) {
      printf("cmty %d has nodes in it (G->cmtyN[%d]=%d) but Ncmty=%d\n",
	     cmty, cmty, G->cmtyN[cmty], G->Ncmty);
      errors++;
    }
  }
  for (cmty=0; cmty < G->Ncmty; cmty++) {
    if ((int) g_hash_table_size(G->cmtyListHash[cmty]) != G->cmtyN[cmty]) {
      printf("cmty %d size mismatch: %d %d\n", cmty,
	     g_hash_table_size(G->cmtyListHash[cmty]), G->cmtyN[cmty]);
      errors++;
    }
  }
  return (errors);
}

void hashCreate(Graph_t G) {
  assert(G->cmtyListHash[0] == NULL  &&  G->cmtyListHash[1] == NULL);
  int c;
  for (c=0 ; c<G->N ; c++) {
    if (DEBUG) assert(G->cmtyListHash[c] == NULL);
    G->cmtyListHash[c] = g_hash_table_new(g_direct_hash,
					  g_direct_equal
					  );
  }
}
void hashClear(Graph_t G) {
  /* Remove all contents from all hashes */
  int c;
  for (c=0 ; c<G->N ; c++) {
    g_hash_table_remove_all(G->cmtyListHash[c]);
  }
}
void hashInit(Graph_t G) {
  /* Initialize hashes from community lists */
  hashClear(G);
  int n;
  for (n=0 ; n<G->N ; n++) {
    int c = G->cmty[n];
    g_hash_table_insert(G->cmtyListHash[c], GINT_TO_POINTER(n), NULL);
  }
}
void hashDestroy(Graph_t G) {
  int c;
  for (c=0 ; c<G->N ; c++) {
    if (DEBUG) assert(G->cmtyListHash[c] != NULL);
    g_hash_table_destroy(G->cmtyListHash[c]);
    G->cmtyListHash[c] = NULL;
  }
  //free(G->cmtyListHash);
}
void hashPrintKeys(GHashTable *HT) {
  void printkey(void *key, void *value, void *data) {
    value=NULL;
    data=NULL;
    printf("%d ", GPOINTER_TO_INT(key));
  }
  g_hash_table_foreach(HT, printkey, NULL);
  printf("\n");
}
void hashInfo(Graph_t G) {
  int (c);
  for (c=0 ; c<G->Ncmty ; c++ ) {
    if (G->cmtyN[c] == 0) continue;
    printf("cmty %2d: ", c);
    GHashTableIter hashIter;
    g_hash_table_iter_init(&hashIter, G->cmtyListHash[c]);
    void *n_p;
    while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
      int n = GPOINTER_TO_INT(n_p);
      printf("%d ", n);
    }
    printf("\n");
  }
}

inline int cmtyN(Graph_t G, int c) {
  return(G->cmtyN[c]);
}
int cmtyIntersect(Graph_t G0, int c0, Graph_t G1, int c1) {
  // We want 0 to be the smaller one, since we linearly iterate
  // over it.
  if (G0->cmtyN[c0] > G1->cmtyN[c1]) {
    Graph_t Gtmp = G0;  G0 = G1 ; G1 = Gtmp;
    int     ctmp = c0;  c0 = c1 ; c1 = ctmp;
  }

  GHashTableIter hashIter;
  int n_intersect = 0;
  void *n_p;

  g_hash_table_iter_init(&hashIter, G0->cmtyListHash[c0]);
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    if (isInCmty(G1, c1, n))
      n_intersect += 1;
    }
  if (DEBUG) assert(n_intersect<=G0->cmtyN[c0] && n_intersect<=G1->cmtyN[c1]);
  return (n_intersect);
}
int cmtyUnion(Graph_t G0, int c0, Graph_t G1, int c1) {
  // We want 0 to be the smaller one, since we linearly iterate
  // over it.
  if (G0->cmtyN[c0] > G1->cmtyN[c1]) {
    Graph_t Gtmp = G0;  G0 = G1 ; G1 = Gtmp;
    int     ctmp = c0;  c0 = c1 ; c1 = ctmp;
  }

  GHashTableIter hashIter;
  int n_union = G1->cmtyN[c1];
  void *n_p;

  g_hash_table_iter_init(&hashIter, G0->cmtyListHash[c0]);
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    if (!isInCmty(G1, c1, n))
      n_union += 1;
    }
  if (DEBUG) assert(n_union <= G0->cmtyN[c0] + G1->cmtyN[c1]);
  return (n_union);
}
int cmtyIsSubset(Graph_t G, int csmall, int cbig) {
  /* is csmall a subset of cbig?  This also allows csmall to be equal
   * to cbig (not a strict superset).  */
  GHashTableIter hashIter;
  GHashTable *hashSmall = G->cmtyListHash[csmall];
  GHashTable *hashBig   = G->cmtyListHash[cbig];
  g_hash_table_iter_init(&hashIter, hashSmall);
  void *n_p;
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    //int n = GPOINTER_TO_INT(n_p);
    int found = g_hash_table_lookup_extended(hashBig,
					     &n_p,
					     NULL, NULL);
    if (!found)
      return(FALSE);
  }
  return (TRUE);
}
int cmtyGetContents(Graph_t G, int c, int *tmp, int *cN) {
  GHashTableIter hashIter;
  void *n_p;

  g_hash_table_iter_init(&hashIter, G->cmtyListHash[c]);
  int i = 0;
  while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    //printf("cGC: %d %d %d\n", c, n, i);
    tmp[i] = n;
    i++;
  }
  *cN = G->cmtyN[c];
  return(1);
}


int find_empty_cmty(Graph_t G) {
  /* Find the lowest numbered empty community
   */
  int c;
  for (c=0 ; c<G->N ; c++) {
    if (G->cmtyN[c] == 0) {
      int n;
      // A bit of error-checking
      if (DEBUG) {
	for (n=0 ; n<G->N ; n++) {
	  if (G->cmty[n] == c) {
	    printf("Inconsistent state: c=%d should be empty (n=%d)\n", c, n);
	    exit(53);
	  }
	}
      }
      return(c);
    }
  }
  return(-1);
}

//int is_in_list()


int q(Graph_t G) {
  /* Number of communities in graph G.
   */
  int q=0;
  int c;
  for (c=0 ; c<G->Ncmty ; c++) {
    if (G->cmtyN[c] > 0)
      q++;
  }
  return (q);
}
double entropy(Graph_t G) {
  /* Information entropy of community allocation of graph G
   * Requires graph to be one-to-one.
   */
  assert(G->oneToOne);
  double N = (double)G->N;
  double H = 0;
  int c;
  for (c=0; c<G->Ncmty ; c++) {
    int n = G->cmtyN[c];
    if (n == 0)
      continue;
    H += n/N * log2((double)(n/N));
  }
  return -H;
}

double mutual_information(Graph_t G0, Graph_t G1) {
  assert(G0->N == G1->N);
  int N = G0->N;
  double MI=0.0;

  int c0, c1, n0, n1, n_shared;
  for (c0=0 ; c0 < G0->Ncmty ; c0++) {
    n0 = G0->cmtyN[c0];
    for (c1=0 ; c1 < G1->Ncmty ; c1++) {
      n1 = G1->cmtyN[c1];

      n_shared = cmtyIntersect(G0, c0, G1, c1);
      if (n_shared == 0)
	continue;
      MI+=(n_shared/(float)N) * log2((((double)n_shared)*N)/(((double)n0)*n1));
    }
  }
  return (MI);
}

inline double h(double p) {
  if ((p==0) || (p==1))
    return 0;
  return -p * log2(p);
}
inline double H(Graph_t G, int c) {
  return (  h((       G->cmtyN[c]) / (double)G->N)
	  + h((G->N - G->cmtyN[c]) / (double)G->N)
         );
}
double H2(Graph_t GX, Graph_t GY, int cX, int cY) {
  double N = (double) GX->N;
  // cX, cY members, cX, cY number in cmty
  //int *cXm = GX->cmtyl[cX];
  //int *cYm = GY->cmtyl[cY];
  int  cX_n = GX->cmtyN[cX];
  int  cY_n = GX->cmtyN[cY];

  //printf("  c %d %d\n", n_intersect_nodes(cXm, cYm, cX_n, cY_n),
  //                 n_union_nodes(cXm, cYm, cX_n, cY_n));
  double hP11 = h((       cmtyIntersect(GX, cX, GY, cY))/N);
  double hP10 = h((cX_n - cmtyIntersect(GX, cX, GY, cY))/N);
  double hP01 = h((cY_n - cmtyIntersect(GX, cX, GY, cY))/N);
  double hP00 = h(( N   - cmtyUnion    (GX, cX, GY, cY))/N);
  if (hP11 + hP00 <= hP01 + hP10)
    return 1/0.;
  double hPY1 = h( (  cY_n) / N);
  double hPY0 = h( (N-cY_n) / N);
  return(hP11+hP00+hP01+hP10 - hPY1 - hPY0);
}
double HX_Ynorm(Graph_t GX, Graph_t GY) {
  double HX_Y_total=0;  // These two are to find the average
  int HX_Y_n=0;
  int cX;
  for (cX=0 ; cX < GX->Ncmty; cX++) {
    if (GX->cmtyN[cX] == 0)
      continue;
    double HX_Yhere=1/0.;
    int cY;
    for (cY=0 ; cY < GY->Ncmty ; cY++) {
      if (GY->cmtyN[cY] == 0)
	continue;
      double H2_this = H2(GX, GY, cX, cY);
      if (H2_this < HX_Yhere)
	HX_Yhere = H2_this;
    }
    if (HX_Yhere == 1/0.)
      HX_Yhere = H(GX, cX);
    double _ = H(GX, cX);
    if (_ == 0) {
      HX_Y_total += 0;
      HX_Y_n += 1;
    }
    else {
      HX_Y_total += HX_Yhere / _;
      HX_Y_n += 1;
    }

  }
  return(HX_Y_total / HX_Y_n);
}

double F1_one(Graph_t G0, int c0, Graph_t G,
	      double *precision_best, double *recall_best) {
  assert(G0->N == G->N);
  //GHashTableIter hashIter;
  //void *n_p;
  double F1_optimal = 0;
  double precision_optimal = 0;
  double recall_optimal = 0;

  int c;
  for (c=0 ; c<G->Ncmty ; c++) {
    if (G->cmtyN[c] == 0)
      continue;
    int n_overlap = 0;

    /* // Find precision.  How many correct ones are in c ? */
    /* g_hash_table_iter_init(&hashIter, G->cmtyListHash[c]); */
    /* while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) { */
    /*   int n = GPOINTER_TO_INT(n_p); */
    /*   if (isInCmty(G, c, n)) */
    /* 	n_overlap += 1; */
    /* } */
    // Find overlaps.
    n_overlap = cmtyIntersect(G0, c0, G, c);
    if (n_overlap > G->cmtyN[c] || n_overlap >G0->cmtyN[c0]) {
      printf("F1_one:: %d %d %d %d %d\n", c0, c, G0->cmtyN[c0], G->cmtyN[c],
	     n_overlap);
    }

    double precision = n_overlap / (double) G->cmtyN[c];
    double recall = n_overlap / (double) G0->cmtyN[c0];
    double F1 = 2 * precision * recall / (precision + recall);
    if (F1 > F1_optimal) {
      F1_optimal = F1;
      precision_optimal = precision;
      recall_optimal = recall;
    }
  }
  if (precision_best != NULL) *precision_best = precision_optimal;
  if (recall_best    != NULL) *recall_best    = recall_optimal;
  return (F1_optimal);
}





double energy(Graph_t G, double gamma) {
  /* Calculate energy using community lists.  Much faster.
   *
   * This function is symmetric.
   */
  if (!G->hasFull) return energy_sparse(G, gamma);
  assert(G->hasFull);
  int c;
  double E=0;

  GHashTableIter hashIterOuter;
  /* GHashTableIter hashIterInner; */

  for (c=0 ; c<G->Ncmty ; c++) {
    // for communities c
    void *n_p;
    g_hash_table_iter_init(&hashIterOuter, G->cmtyListHash[c]);
    while (g_hash_table_iter_next(&hashIterOuter, &n_p, NULL)) {
      int n = GPOINTER_TO_INT(n_p);
      // Do symmetric: both directions.
      E += energy_cmty_n(G, gamma, c, n);
    }
  }
  return(E);
}


double energy_sparse(Graph_t G, double gamma) {
  /* Calculate energy using community lists.  Much faster.
   *
   * This function is symmetric.
   */
  assert(G->hasSparse);
  double E=0;

  GHashTableIter hashIter;


  int c;
  for (c=0 ; c<G->Ncmty ; c++) {
    // for communities c
    void *n_p;
    g_hash_table_iter_init(&hashIter, G->cmtyListHash[c]);
    // For each particle in the community
    while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
      int n = GPOINTER_TO_INT(n_p);
      // Add up energy of that particle to the community.
      E += energy_cmty_n_sparse(G, gamma, c, n);
    }
  }
  return(E);
}




double energy_cmty(Graph_t G, double gamma, int c) {
  /* Calculate the energy of only one community `c`.
   *
   * This function is symmetric.
   */
  assert(G->hasFull);
  double E=0;

  GHashTableIter hashIterOuter;
  //GHashTableIter hashIterInner;

  void *n_p;
  g_hash_table_iter_init(&hashIterOuter, G->cmtyListHash[c]);
  while (g_hash_table_iter_next(&hashIterOuter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    E += energy_cmty_n(G, gamma, c, n);
  }
  return(E);
}

double energy_cmty_n(Graph_t G, double gamma, int c, int n) {
  /* Calculate the energy of only one community `c`, if it had node n
   * in it.  Node n does not have to actually be in that community.
   *
   * This function is unidirectional (node n -> cmty c)
   */
  assert(G->hasFull);
  imatrix_t attractions=0;
  imatrix_t repulsions =0;

  GHashTableIter hashIterInner;

  // for community c
  void *m_p;
  g_hash_table_iter_init(&hashIterInner, G->cmtyListHash[c]);
  while (g_hash_table_iter_next(&hashIterInner, &m_p, NULL)) {
    int m = GPOINTER_TO_INT(m_p);
    if (m == n)
      continue;
    if (G->rmatrix == NULL) {
      imatrix_t interaction = G->imatrix[n*G->N + m];
      if (interaction > 0)
	repulsions  += interaction;
      else
	attractions += interaction;
    }
    else {
      attractions += G->imatrix[n*G->N + m];
      repulsions += G->rmatrix[n*G->N + m];
    }
  }
  return(.5 * (attractions + gamma*repulsions));
}

double energy_cmty_n_sparse(Graph_t G, double gamma, int c, int n) {
  /* Calculate the energy of only one community `c`, if it had node n
   * in it.  Node n does not have to actually be in that community.
   *
   * This function is unidirectional (node n -> cmty c)
   */
  assert(G->hasSparse);
  imatrix_t attractions=0;
  imatrix_t repulsions =0;

  int j;
  int nDefinedI = 0;
  //int nDefinedR = 0;
  // For each adjoining particle in the list:
  for (j=0 ; j<G->simatrixN[n] ; j++) {
    //int m = G->simatrixId[n*G->simatrixLen + j];
    int m = G->simatrixIdl[n][j];
    if (m == n)
      continue;
    if (! isInCmty(G, c, m))
      continue;

    nDefinedI++;
    if (G->srmatrix == NULL) {
      imatrix_t interaction = G->simatrix[n*G->simatrixLen + j];
      if (interaction > 0)
	repulsions  += interaction;
      else
	attractions += interaction;
    }
    else {
      attractions += G->simatrix[n*G->simatrixLen + j];
      repulsions += G->srmatrix[n*G->simatrixLen + j];
    }
  }
  int nUnDefinedI = G->cmtyN[c] - nDefinedI;
  int nUnDefinedR = nUnDefinedI;

  // -1 comes from self interaction not being counted as undefined.
  if (isInCmty(G, c, n)) {
    nUnDefinedI -= 1;
    nUnDefinedR -= 1;
  }

  if (G->srmatrix == NULL) {
    attractions += nUnDefinedI * G->simatrixDefault;
    repulsions  += nUnDefinedR * G->srmatrixDefault;

    /* imatrix_t interaction = G->srmatrixDefault; */
    /* if (interaction > 0) */
    /*   repulsions  += nUnDefinedI * G->srmatrixDefault; */
    /* else */
    /*   attractions += nUnDefinedI * G->simatrixDefault; */
  }
  else if (G->srmatrixDefaultOnlyDefined != 0) {
    attractions += nUnDefinedI * G->simatrixDefault;
    repulsions  += nUnDefinedR * G->srmatrixDefault;
    repulsions  += nDefinedI * G->srmatrixDefaultOnlyDefined;
  }
  else {
    attractions += nUnDefinedI * G->simatrixDefault;
    repulsions  += nUnDefinedR * G->srmatrixDefault;
  }

  double E = .5 * (attractions + gamma*repulsions);
  /* printf(" e_c_n_s %d %d(%d,%d)\n", c, n, G->cmtyN[c], nUnDefined); */
  //if (DEBUG && E != energy_cmty_n(G, gamma, c, n)) {
  if (DEBUG && G->hasFull) {
    double E2 = energy_cmty_n(G, gamma, c, n);
    if ( fabs(E-E2)>.01  && fabs(E-E2)/E > .0001) {
      printf(" e_c_n_s %d %d(%d,%d) (%d) %f %f\n", c, n, G->cmtyN[c],
	     nUnDefinedI,
	     isInCmty(G, c, n),
	     E, E2);
      //assert(!cmtyListCheck(G));
      assert(0);
    }
  }
  return(E);
}
double energy_cmty_n_which(Graph_t G, double gamma, int c, int n) {
  if (G->hasSparse) return energy_cmty_n_sparse(G, gamma, c, n);
  return energy_cmty_n(G, gamma, c, n);
}



double energy_cmty_cmty(Graph_t G, double gamma, int c1, int c2) {
  /* Total energy of interaction between two communities.
   *
   * This function is asymmetric, c1 -> c2
   */
  assert(G->hasFull);
  double E=0;
  GHashTableIter hashIterOuter;
  void *n_p;

  g_hash_table_iter_init(&hashIterOuter, G->cmtyListHash[c1]);
  while (g_hash_table_iter_next(&hashIterOuter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    E += energy_cmty_n(G, gamma, c2, n);
    /* printf("ecc %d %d %f\n", c2, n, E); */
  }
  return (E);
}
double energy_cmty_cmty_sparse(Graph_t G, double gamma, int c1, int c2) {
  /* Total energy of interaction between two communities.
   *
   * This function is asymmetric, c1 -> c2
   */
  assert(G->hasSparse);
  double E=0;
  GHashTableIter hashIterOuter;
  void *n_p;

  g_hash_table_iter_init(&hashIterOuter, G->cmtyListHash[c1]);
  while (g_hash_table_iter_next(&hashIterOuter, &n_p, NULL)) {
    int n = GPOINTER_TO_INT(n_p);
    E += energy_cmty_n_sparse(G, gamma, c2, n);
    /* printf("eccs %d %d %f\n", c2, n, E); */
  }
  return (E);
}
double energy_n(Graph_t G, double gamma, int n) {
  /* Energy of particle n in its own community.
   *
   * This function is asymmetric, n -> its community.
   */
  assert(G->hasFull);
  assert(G->oneToOne);
  int c = G->cmty[n];
  return energy_cmty_n(G, gamma, c, n);
}

double energy_cmty_cmty_xor_sparse(Graph_t G, double gamma, int c1, int c2) {
  /*
   *
   * This function is unidirectional c1 -> c2.
   */
  assert (G->hasSparse);

  double attractions=0;
  double repulsions=0;
  int n_intersect = cmtyIntersect(G, c1, G, c2);
  int n1only = (cmtyN(G, c1) - n_intersect);
  int n2only = (cmtyN(G, c2) - n_intersect);
  int nUnDefinedI = n1only * n2only;

  GHashTableIter hashIter1;
  void *n1_p;

  g_hash_table_iter_init(&hashIter1, G->cmtyListHash[c1]);
  while (g_hash_table_iter_next(&hashIter1, &n1_p, NULL)) {
    int n1 = GPOINTER_TO_INT(n1_p);
    if (isInCmty(G, c2, n1))
      continue;

    int i;
    for (i=0 ; i<G->simatrixN[n1] ; i++) {
      int n2 = G->simatrixId[i];
      if (n2 == n1)
	continue;
      if (isInCmty(G, c1, n2))
	continue;
      if (!isInCmty(G, c2, n2))
	continue;

      // energy between n1 and n2

      nUnDefinedI -= 1;
      if (G->srmatrix == NULL) {
        imatrix_t interaction = G->simatrix[n1*G->simatrixLen + i];
        if (interaction > 0)
      	repulsions  += interaction;
        else
      	attractions += interaction;
      }
      else {
        attractions += G->simatrix[n1*G->simatrixLen + i];
        repulsions += G->srmatrix[n1*G->simatrixLen + i];
      }

      assert(0); // Needs to be upgraded to handle srmatrix.
    }
  }
  attractions += nUnDefinedI * G->simatrixDefault;
  repulsions  += nUnDefinedI * G->srmatrixDefault;

  double E = .5 * (attractions + gamma*repulsions);
  return(E);

}



int greedy_naive(Graph_t G, double gamma) {
  /* OBSELETE minimization routine.
   */
  assert(0);
  assert(G->hasFull);
  int changes=0;
  int i, n;
  // Loop over particles
  for (i=0 ; i<G->N ; i++) {
    n = G->randomOrder[i];
    int oldcmty  = G->cmty[n];
    int bestcmty = G->cmty[n];
    double Ebest = energy(G, gamma);
    /* printf("Partile %d, old community %d\n", i, oldcmty); */
    cmtyListRemove(G, oldcmty, n);


    int newcmty;
    for (newcmty=0 ; newcmty<G->N ; newcmty++) {

      // Try partiicle in each new cmty
      if (newcmty == oldcmty)
	continue;
      if (G->cmtyN[newcmty] == 0) {
	continue;
      }

      double Enew;
      //G->cmty[n] = newcmty;
      cmtyListAdd(G, newcmty, n);
      Enew = energy(G, gamma);
      cmtyListRemove(G, newcmty, n);

      if (Enew < Ebest) {
      /* printf("  Better option for particle %d: %d %d %d\n", */
      /* 	    i, oldcmty, bestcmty, newcmty); */
      bestcmty = newcmty;
      Ebest = Enew;
      }
    }
    //G->cmty[n] = bestcmty;
    cmtyListAdd(G, bestcmty, n);
    if (oldcmty != bestcmty) {
      // Change community
      changes += 1;
      /* printf("particle %4d: cmty change %4d->%4d\n",  */
      /*        i, oldcmty, bestcmty); */
      //G->cmtyN[oldcmty]  --;
      //G->cmtyN[bestcmty] ++;
    }
  }
return (changes);
}

int greedy(Graph_t G, double gamma) {
  /* Core minimization routine.  Do one sweep, moving each particle
   * (in order of G->randomOrder into the community that most lowers the
   * energy.
   */
  if (G->hasSparse) return(greedy_sparse(G, gamma));
  assert(G->hasFull);
  int changes=0;
  int nindex, n;
  // Loop over particles
  for (nindex=0 ; nindex<G->N ; nindex++) {
    n = G->randomOrder[nindex];
    // Keep a record of the running best community to move to.
    // Default to current community (no moving).
    double deltaEbest = 0.0;
    int bestcmty = G->cmty[n];

    // Store our old community and energy change when we remove a
    // particle from the old community.  We see if (energy from
    // removing from old community + energy from adding to new
    // community) is less than deltaEbest to see where we should move.
    int oldcmty  = G->cmty[n];
    double deltaEoldCmty = - energy_cmty_n(G, gamma, oldcmty, n);


    // Try particle in each new cmty.  Accept the new community
    // that has the lowest new energy.
    // There are various ways of doing this inner loop:

    // Method 1 (all other communities) //
    /* int newcmty; */
    /* for (newcmty=0 ; newcmty<G->Ncmty ; newcmty++) { */

    // Method 2 (only interacting cmtys, fixed order) //
    int m;
    for (m=0 ; m<G->N ; m++) {
      /* if (G->imatrix[G->N*n+m] != 500) */
      /* 	printf("  %d %d %f\n", n, m, G->imatrix[G->N*n+m]); */

      if (G->imatrix[n*G->N + m] > 0)
    	continue;
      int newcmty = G->cmty[m];

    // Method 3 (only interacting cmtys, random order) //
    /* int mindex, m; */
    /* for (mindex=0 ; mindex<G->N ; mindex++) { */
    /*   m = G->randomOrder2[mindex]; */
    /*   if (G->imatrix[n*G->N + m] > 0) */
    /* 	continue; */
    /*   int newcmty = G->cmty[m]; */

      if (newcmty == oldcmty)
	continue;
      if (G->cmtyN[newcmty] == 0) {
	continue;
      }

      double deltaEnewCmty = energy_cmty_n(G, gamma, newcmty, n);

      // Our conditional on if we want to move to this new place.  If
      // we do, update our bestcmty and deltaEbest to say so.
      if (deltaEoldCmty + deltaEnewCmty < deltaEbest) {
	bestcmty = newcmty;
	deltaEbest = deltaEoldCmty + deltaEnewCmty;
      }
    }
    // Is it better to move a particle into an _empty_ community?
    if (deltaEoldCmty < deltaEbest) {
      bestcmty = find_empty_cmty(G);
      // deltaEbest = deltaEoldCmty;  // Not needed (not used after this)
      assert(bestcmty != -1);
    }
    if (oldcmty != bestcmty) {
      cmtyMove(G, n, oldcmty, bestcmty);
      changes += 1;
    }
  }
  return (changes);
}


int greedy_sparse(Graph_t G, double gamma) {
  /* Core minimization routine.  Do one sweep, moving each particle
   * (in order of G->randomOrder into the community that most lowers the
   * energy.
   */
  assert(G->hasSparse);
  int changes=0;
  int nindex, n;

  /* double E_avg = energy(G, gamma) / G->N; */

  // Loop over particles
  for (nindex=0 ; nindex<G->N ; nindex++) {
    n = G->randomOrder[nindex];

    /* float E_particle = energy_n(G, gamma, n); */
    /* if (E_particle < E_avg) */
    /*   continue; */

    // Keep a record of the running best community to move to.
    // Default to current community (no moving).
    double deltaEbest = 0.0;
    int bestcmty = G->cmty[n];

    // Store our old community and energy change when we remove a
    // particle from the old community.  We see if (energy from
    // removing from old community + energy from adding to new
    // community) is less than deltaEbest to see where we should move.
    int oldcmty  = G->cmty[n];
    double deltaEoldCmty = - energy_cmty_n_sparse(G, gamma, oldcmty, n);

    // Try particle in each new cmty.  Accept the new community
    // that has the lowest new energy.
    // There are various ways of doing this inner loop:

    SetClear(G->seenList);
    int j;
    for(j=0 ; j<G->simatrixN[n] ; j++) {
      if (G->simatrix[G->simatrixLen*n + j] >= 0)
      	continue;
      int m = G->simatrixId[G->simatrixLen*n + j];
      int newcmty = G->cmty[m];


      if (newcmty == oldcmty)
	continue;
      if (G->cmtyN[newcmty] == 0) {
	continue;
      }
      if (SetContains(G->seenList, newcmty))
      	continue;
      SetAdd(G->seenList, newcmty);

      double deltaEnewCmty = energy_cmty_n_sparse(G, gamma, newcmty, n);

      // Our conditional on if we want to move to this new place.  If
      // we do, update our bestcmty and deltaEbest to say so.
      if (deltaEoldCmty + deltaEnewCmty < deltaEbest) {
	bestcmty = newcmty;
	deltaEbest = deltaEoldCmty + deltaEnewCmty;
      }
    }
    // Is it better to move a particle into an _empty_ community?
    if (deltaEoldCmty < deltaEbest) {
      bestcmty = find_empty_cmty(G);
      // deltaEbest = deltaEoldCmty;  // Not needed (not used after this)
      assert(bestcmty != -1);
    }
    if (oldcmty != bestcmty) {
      cmtyMove(G, n, oldcmty, bestcmty);
      changes += 1;
    }
  }
  return (changes);
}


int overlapAdd(Graph_t G, double gamma) {
  /* Do a minimization attempt, but adding particles to new
   * overlapping communities.  However, we can't remove particles from
   * their original communities.
   *
   * One principle in this function is that G->cmty[n] lists the
   * _original_ community each particle was in.
   *
   * This makes a round of adding particles to communities.
   */
  int changes = 0;
  G->oneToOne = 0;
  assert(G->hasSparse);

  /* int nindex, n; */
  /* // Loop over particles */
  /* for (nindex=0 ; nindex<G->N ; nindex++) { */
  /*   n = G->randomOrder[nindex]; */

    /* // Method 1 (all other communities) // */
    /* int c; */
    /* for (c=0 ; c<G->Ncmty ; c++) { */
    /*   if (G->cmtyN[c] == 0) */
    /* 	continue; */

    /* // Method 2 (all other communities, random order) // */
    /* int cindex, c; */
    /* for (cindex=0 ; cindex<G->N ; cindex++) { */
    /*   c = G->randomOrder2[cindex]; */
    /*   if (c >= G->Ncmty) */
    /* 	continue; */
    /*   if (G->cmtyN[c] == 0) */
    /* 	continue; */

    /* // Method 3 (all neighboring communities using sparse matrix) // */
    /* LListClear(G->seenList); */
    /* int mindex; */
    /* for (mindex=0 ; mindex<G->simatrixN[mindex] ; mindex++) { */
    /*   int m = G->simatrixId[mindex]; */
    /*   int c = G->cmty[m]; */
    /*   if (LListContains(G->seenList, c)) */
    /* 	continue; */
    /*   LListAdd(G->seenList, c); */

    /* // Method 4 (all neighboring cmtys using sparse matrix, correctly) // */
    /* LListClear(G->seenList); */
    /* int mindex; */
    /* for (mindex=0 ; mindex<G->simatrixN[mindex] ; mindex++) { */
    /*   int m = G->simatrixId[mindex]; */
    /*   int c; */
    /* for (c=0 ; c<G->Ncmty ; c++) { */
    /*   if (! isInCmty(G, c, m)) */
    /* 	continue; */
    /*   if (LListContains(G->seenList, c)) */
    /* 	continue; */
    /*   LListAdd(G->seenList, c); */


  // Method Beta - loop over communities first.
  int c;
  // For each community
  for (c=0 ; c<G->Ncmty ; c++) {
    if (G->cmtyN[c] == 0)
      continue;
    SetClear(G->seenList);
    int cN;
    cmtyGetContents(G, c, G->tmp, &cN);
    int mindex;
    // for every particle m in community c
    for (mindex=0 ; mindex<cN ; mindex++) {
      int m;
      m = G->tmp[mindex];
    // for every neighboring particle of n of particles m
    int nindex;
    for (nindex=0 ; nindex<G->simatrixN[m] ; nindex++) {
      if (G->simatrix[m*G->simatrixLen + nindex] >= 0)
	continue;
      int n;
      n = G->simatrixIdl[m][nindex];
      if (SetContains(G->seenList, n))
      	continue;
      SetAdd(G->seenList, c);



      // If this is in the original comminty, can skip it.
      if (c == G->cmty[n])
	continue;
      // Skip if already in this community.
      if (isInCmty(G, c, n))
	continue;

      // Should this be added to community?
      double deltaE = energy_cmty_n_which(G, gamma, c, n);
      if (deltaE < 0) {
	if (DEBUGLISTS) printf("    Adding in overlapmin_add %d %d\n", c, n);
	cmtyListAddOverlap(G, c, n);
	changes++;
      }

    }  // Extra brace
    }
  }
  return (changes);
}
int overlapRemove(Graph_t G, double gamma) {
  /* Do a minimization attempt, but removing particles from
   * overlapping communities.  However, we can't remove particles from
   * their original communities.
   *
   * One principle in this function is that G->cmty[n] lists the
   * _original_ community each particle was in.
   *
   * This makes a single round of removing particles from each community.
   */
  int changes = 0;
  G->oneToOne = 0;

  int c;
  // Loop over communities
  for (c=0 ; c<G->Ncmty ; c++) {
    if (G->cmtyN[c] == 0)
      continue;

    // Loop over particles within that community
    int i;
    int cN;
    cmtyGetContents(G, c, G->tmp, &cN);
    for (i=0 ; i<cN ; i++) {
      int n = G->tmp[i];

      // If this is in the original comminty, can skip the tests.
      if (c == G->cmty[n])
	continue;

      // Should this be removed from the community?
      double deltaE = energy_cmty_n_which(G, gamma, c, n);
      if (deltaE > 0) {
	if (DEBUGLISTS) printf("    Removing in overlapmin_rem %d %d\n", c, n);
	cmtyListRemoveOverlap(G, c, n);
	changes++;
      }
    }
  }
  return (changes);
}
int overlapRemove2(Graph_t G, double gamma) {
  /* Do a minimization attempt, but adding particles to new
   * overlapping communities.  However, we can't remove particles from
   * their original communities.
   *
   * This function does not pay attention to the G->cmty[n] lists the
   * _original_ community each particle was in, and thus it can be
   * removed from the original community (thus leaving the node in
   * *no* communities).
   *
   * This makes a single round of removing particles from each community.
   */
  int changes = 0;
  G->oneToOne = 0;
  G->hasPrimaryCmty = 0;

  int c;
  // Loop over communities
  for (c=0 ; c<G->Ncmty ; c++) {
    if (G->cmtyN[c] == 0)
      continue;

    // Loop over particles within that community
    GHashTableIter hashIter;
    void *n_p;
    g_hash_table_iter_init(&hashIter, G->cmtyListHash[c]);
    while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
      int n = GPOINTER_TO_INT(n_p);

      // Should this be removed from the community?
      double deltaE = energy_cmty_n_which(G, gamma, c, n);
      if (deltaE > 0) {
	if (DEBUGLISTS) printf("    Removing in ovRemove2 %d %d\n", c, n);
	g_hash_table_iter_remove(&hashIter);
	_cmtyListRemoveOverlap(G, c, n);
	G->cmty[n] = NO_CMTY;
	changes++;
      }
    }
  }
  return (changes);
}

int anneal(Graph_t G, double gamma, double beta,
	   int steps, double deltabeta) {
  int changes=0;
  int step;

  for (step=0 ; step<steps ; step++) {
    beta += deltabeta;


    // Random node
    int n = G->N * genrand_real2();
    // Random community
    int c = G->N * genrand_real2();
    int c_old = G->cmty[n];
    // Random chance of trying a new communty
    if (genrand_real2() < .0001 ) {
      c = find_empty_cmty(G);
      if (c == -1)
	continue;
    }

    // If already in this community, skip it.
    if (c == c_old)
      continue;
    float deltaE =   energy_cmty_n(G, gamma, c, n)
                   - energy_cmty_n(G, gamma, c_old, n);

    double x = exp(- beta * deltaE);
    double ran = genrand_real2();
    if (ran < x) {
      // accept
      cmtyMove(G, n, c_old, c);
      changes += 1;
    }
    else {
      // reject
    }
  }
  return (changes);
}



int combine(Graph_t G, double gamma) {
  /* Attempt to merge communities to get a lower energy assignment.
   * Pairwise attempt to merge all.
   */
  if (G->hasSparse) return(combine_sparse(G, gamma));
  assert(G->oneToOne);
  assert(G->hasFull);
  int changes = 0;
  // Move particles from c2 into c1
  int i1, i2, c1, c2;
  //printf("gamma: %f\n", gamma);
  /* for (c1=0 ; c1<G->Ncmty-1 ; c1++) { */
  /*   if (G->cmtyN[c1] == 0) */
  /*     continue; */
  /*   int bestcmty = c1; */
  /*   double deltaEbest = 0; */
  /*   for (c2=c1+1 ; c2<G->Ncmty ; c2++) { */
  /*     if (G->cmtyN[c2] == 0) */
  /* 	continue; */
  for (i1=0 ; i1<G->N ; i1++) {
    c1 = G->randomOrder[i1];
    if (G->cmtyN[c1] == 0)
      continue;

    int bestcmty = c1;
    double deltaEbest = 0;

    for (i2=0 ; i2<G->N ; i2++) {
      c2 = G->randomOrder2[i2];
      if (c1 <= c2)
  	continue;
      if (G->cmtyN[c2] == 0)
  	continue;
      /* //double Eold = energy(G, gamma); */
      /* double Eold = energy_cmty(G, gamma, c1) + energy_cmty(G, gamma, c2);*/


      // Calculate change of energy if we moved c1 into c2
      double deltaE = 0;
      deltaE += energy_cmty_cmty(G, gamma, c1, c2);
      deltaE += energy_cmty_cmty(G, gamma, c2, c1);

      // Do we accept this change?
      if (deltaE < deltaEbest) {
	bestcmty = c2;
	deltaEbest = deltaE;
      }
    }
    // No change
    if (c1 == bestcmty)
      continue;
    //Move all from bestcmty into c1
    cmtyMoveAll(G, bestcmty, c1);
    changes += 1;
  }
  return (changes);
}


int combine_sparse(Graph_t G, double gamma) {
  /* Attempt to merge communities to get a lower energy assignment.
   * Pairwise attempt to merge all.
   */
  assert(G->oneToOne);
  assert(G->hasSparse);
  assert(G->hasPrimaryCmty);
  int changes = 0;
  // Move particles from c2 into c1
  //int i1, i2, c1, c2;
  int c1;
  //printf("gamma: %f\n", gamma);
  for (c1=0 ; c1<G->Ncmty-1 ; c1++) {
    if (G->cmtyN[c1] == 0)
      continue;
    int bestcmty = c1;
    double deltaEbest = 0;


  /* for (i1=0 ; i1<G->N ; i1++) { */
  /*   c1 = G->randomOrder[i1]; */
  /*   if (G->cmtyN[c1] == 0) */
  /*     continue; */

  /*   int bestcmty = c1; */
  /*   double deltaEbest = 0; */

  /*   for (i2=0 ; i2<G->N ; i2++) { */
  /*     c2 = G->randomOrder2[i2]; */
  /*     if (c1 <= c2) */
  /*   	continue; */
  /*     if (G->cmtyN[c2] == 0) */
  /*   	continue; */
      /* //double Eold = energy(G, gamma); */
      /* double Eold = energy_cmty(G, gamma, c1) + energy_cmty(G, gamma, c2);*/


    /* for (c2=c1+1 ; c2<G->Ncmty ; c2++) { */
    /*   if (G->cmtyN[c2] == 0) */
    /* 	continue; */


    SetClear(G->seenList);
    GHashTableIter hashIter;
    void *n_p;
    g_hash_table_iter_init(&hashIter, G->cmtyListHash[c1]);
    while (g_hash_table_iter_next(&hashIter, &n_p, NULL)) {
      int n = GPOINTER_TO_INT(n_p);
      //printf("n %d %d %d\n", c1, ii, n);
    int j;
    for(j=0 ; j<G->simatrixN[n] ; j++) {
      if (G->simatrix[G->simatrixLen*n + j] >= 0)
      	continue;
      int m = G->simatrixId[G->simatrixLen*n + j];
      int c2 = G->cmty[m];
      //printf("  m %d %d %d %d\n", c1, j, m, c2);
      if (c1==c2 || SetContains(G->seenList, c2)) {
    	//printf("already in: %d\n", c2);
      	continue;
      }
      SetAdd(G->seenList, c2);
      //printf("adding: %d\n", c2);


      // Calculate change of energy if we moved c1 into c2
      double deltaE = 0;
      deltaE += energy_cmty_cmty_sparse(G, gamma, c1, c2);
      deltaE += energy_cmty_cmty_sparse(G, gamma, c2, c1);

      //printf("  e %d %d %f %d %f\n", c1, c2, deltaE, bestcmty, deltaEbest);
      // Do we accept this change?
      if (deltaE < deltaEbest) {
	//printf("better one\n");
	bestcmty = c2;
	deltaEbest = deltaE;
      }
    } // extra brace
    }
    //printf("loop done\n");

    // No change
    if (c1 == bestcmty)
      continue;
    //printf("  c %d %d combining\n", c1, c2);
    //Move all from bestcmty into c1
    cmtyMoveAll(G, bestcmty, c1);
    changes += 1;
  }
  return (changes);
}


int combine_sparse_overlap(Graph_t G, double gamma) {
  /* Attempt to merge communities to get a lower energy assignment.
   * Pairwise attempt to merge all.
   *
   * This version allows the communties to be overlapping.
   */
  G->oneToOne = 0;
  assert(G->hasSparse);
  //assert(G->hasPrimaryCmty);
  int changes = 0;
  // Move particles from c2 into c1
  //int i1, i2, c1, c2;
  int c1;
  //printf("gamma: %f\n", gamma);
  // For communities c1
  for (c1=0 ; c1<G->Ncmty-1 ; c1++) {
    if (G->cmtyN[c1] == 0)
      continue;
    int bestcmty = c1;
    double deltaEbest = 0;

    int c2;
    for (c2=c1+1 ; c2<G->Ncmty ; c2++) {

    /* SetClear(G->seenList); */
    /* int ii; */
    /* // For every neighboring particle n */
    /* for (ii=0 ; ii<G->cmtyN[c1] ; ii++ ) { */
    /*   int n = G->cmtyl[c1][ii]; */
    /*   //printf("n %d %d %d\n", c1, ii, n); */
    /* int j; */
    /* // For every interacting particle m of neighboring particles n */
    /* for(j=0 ; j<G->simatrixN[n] ; j++) { */
    /*   if (G->simatrix[G->simatrixLen*n + j] >= 0) */
    /*   	continue; */
    /*   int m = G->simatrixId[G->simatrixLen*n + j]; */
    /*   int c2 = G->cmty[m]; */
    /*   //printf("  m %d %d %d %d\n", c1, j, m, c2); */
    /*   if (c1==c2 || SetContains(G->seenList, c2)) { */
    /* 	//printf("already in: %d\n", c2); */
    /*   	continue; */
    /*   } */
    /*   SetAdd(G->seenList, c2); */
    /*   //printf("adding: %d\n", c2); */



      // Calculate change of energy if we moved c1 into c2
      double deltaE = 0;
      deltaE += energy_cmty_cmty_xor_sparse(G, gamma, c1, c2);
      deltaE += energy_cmty_cmty_xor_sparse(G, gamma, c2, c1);

      //printf("  e %d %d %f %d %f\n", c1, c2, deltaE, bestcmty, deltaEbest);
      // Do we accept this change?
      if (deltaE < deltaEbest) {
	//printf("better one\n");
	bestcmty = c2;
	deltaEbest = deltaE;
      }
    /* } // extra brace */
    }
    //printf("loop done\n");

    // No change
    if (c1 == bestcmty)
      continue;
    //printf("  c %d %d combining\n", c1, c2);


    cmtyMoveAllOverlap(G, bestcmty, c1);
    changes += 1;
  }
  return (changes);
}



int remap(Graph_t G) {
  /* Moves all the communities to the lowest numbered continuous
     segments.
   */
  int changes=0;
  int c;
  for (c=G->Ncmty-1 ; c>=0 ; c--) {
    // For each community, starting with the highest.
    if (G->cmtyN[c] == 0)
      // Ignore already-empty communities
      continue;
    int cNew = find_empty_cmty(G);
    if (cNew >= c)
      break;
    if (cNew == -1)
      break;

    // Do the actual remapping:
    cmtyMoveAllOverlap(G, c, cNew);  // Need to use overlap since
    changes++;
  }
  return(changes);
}


