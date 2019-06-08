#include <stdint.h>


/*----------------------------- Structures --------------------------------*/

typedef short DCTELEM;

/* Data structure for a sparse bin.
*/
typedef struct SparseBinSt {
    uint32_t value;
    uint16_t index;
    struct SparseBinSt *prev;
    struct SparseBinSt *next;
} *SparseBin;


/* Data structure for a sparse histogram.
*/
typedef struct SparseHistogramSt {
    SparseBin first;
    SparseBin last;
    uint64_t size;
} *SparseHistogram;


/* ------------------------ Global variables ----------------------------- */

static SparseHistogram histogram = NULL;


/* ------------------ External function prototypes ----------------------- */

SparseHistogram CreateSparseHistogram();
void WriteSparseHistogramFile(char *filename, SparseHistogram histogram);
void WriteSparseHistogram(FILE *fp, SparseHistogram histogram);
void DestroySparseHistogram(SparseHistogram *histogram);
void ExtractMotionFeatures(DCTELEM *prev, DCTELEM *cur, DCTELEM *next, 
                           int width, int height, 
                           SparseHistogram histogram);
