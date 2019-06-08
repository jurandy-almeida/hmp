// HMP python wrapper.


#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "numpy/arrayobject.h"

#include <hmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <libavformat/avformat.h>


static const char *filename = NULL;

static PyObject *HMPError;
						

/*------------------------- Macros and constants  -------------------------*/

#define INPUT_BUFFER_SIZE 4096


/* Given a pointer, FREE deallocates the space used by it.
*/
#define FREE(pointer) {if (pointer != NULL) {free(pointer); pointer = NULL; }}


/* -------------------- Local function prototypes ------------------------ */

void SparseHistogram2PyArray(PyArrayObject ** npy_arr);

static int fatal_error(char * fmt, ...);
static DCTELEM * copy_dct_coeff(DCTELEM * dct_coeff, 
                                int width, int height);
static int decode_frame(AVCodecContext *pCodecCtx, 
                        AVFrame *pFrame, 
                        AVPacket *packet,
                        int frame_count);
static int parse_video(PyArrayObject ** npy_arr);
						

/*----------------------------- Routines ----------------------------------*/

void SparseHistogram2PyArray(PyArrayObject ** npy_arr)
{
    SparseBin bin;

    // Initialize arrays. 
    if (! (*npy_arr)) {
        npy_intp dim[1] = { 6075 };
        *npy_arr = (PyArrayObject *) PyArray_ZEROS(1, dim, NPY_DOUBLE, 0);
    }

    if (histogram && histogram->size > 0)
        for (bin = histogram->first; bin != NULL; bin = bin->next)
            *((double*)PyArray_GETPTR1(*npy_arr, bin->index)) = (double) bin->value / histogram->size;
}


static int fatal_error(char * fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stdout, "Error: ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
  
    return -1;
}


/* Copy DCT coefficients for each macroblock in this frame.
*
* DCT coefficients are stored in field dct_coeff of AVFrame.
* DCT coeffs are stored in dct_coeff only if FF_DEBUG_DCT_COEFF is
* true. To activate this debug option use method setDebugDct().
* dct_coeff is an array where information is grouped in sub-bloks
* of dimension 6 * 64. Each block of data is componed of 4*64 element
* to represent Y DCT coeff, followed by 64 element to represent Cb DCT coeff
* and finally last 64 elements to represent Cr DCT coeffs. 
*/
static DCTELEM * copy_dct_coeff(DCTELEM * dct_coeff, 
                                int width, int height) 
{
    const int mb_width  = (width + 15) / 16;
    const int mb_height = (height + 15) / 16;
    const int mb_stride = mb_width + 1;
    const int mb_array_size = mb_stride * mb_height;
    DCTELEM *ptr = NULL;

    if (dct_coeff) {
        size_t size = 64 * mb_array_size * sizeof(DCTELEM) * 6;
        ptr = (DCTELEM *)malloc(size);
        memcpy(ptr, dct_coeff, size);
    }

    return ptr;
}


static int decode_frame(AVCodecContext *pCodecCtx, 
                        AVFrame *pFrame, 
                        AVPacket *packet,
                        int frame_count)
{
    int bytesDecoded;
    int frameFinished;

    /* Decode the next chunk of data. */
    bytesDecoded = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);

    /* Was there an error? */
    if (bytesDecoded < 0)
        return fatal_error("Error while decoding frame %d.", frame_count);

    if (packet->data) {
        packet->size -= bytesDecoded;
        packet->data += bytesDecoded;
    }
	
    return frameFinished;
}

static int parse_video(PyArrayObject ** npy_arr) 
{
    AVCodec *pCodec = NULL;
    AVCodecContext *pCodecCtx = NULL;  
    AVCodecParserContext *pCodecParserCtx = NULL;  

    FILE *fp;
    AVFrame *pFrame;
    
    uint8_t input_buffer[INPUT_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

    uint8_t *data;  
    size_t data_size;
    AVPacket packet;  
    int bytesParsed;

    /* Register all codecs. */
    avcodec_register_all();  
  
    /* Set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams). */
    memset(input_buffer + INPUT_BUFFER_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
  
    av_init_packet(&packet);
  
    /* Find the MPEG-2 video decoder. */  
    pCodec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);  
    if (!pCodec)
        return fatal_error("Codec not found.");

    pCodecParserCtx = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);  
    if (!pCodecParserCtx)  
        return fatal_error("Could not allocate video parser context.");  

    pCodecCtx = avcodec_alloc_context3(pCodec);  
    if (!pCodecCtx)
        return fatal_error("Could not allocate video codec context.");  

    /* Debug the DCT coefficients. */  
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "debug", "dct_coeff", 0);

    /* Inform the codec that we can handle truncated bitstreams -- i.e.,
       bitstreams where frame boundaries can fall in the middle of packets.
     */
    if (pCodec->capabilities & CODEC_CAP_TRUNCATED)
        pCodecCtx->flags |= CODEC_FLAG_TRUNCATED;
	
    /* Open codec. */	      
    if (avcodec_open2(pCodecCtx, pCodec, &opts) < 0)
        return fatal_error("Could not open codec.");  

    /* Allocate video frame. */
    pFrame = avcodec_alloc_frame();

    /* Open input file. */
    fp = fopen(filename, "rb");  
    if (!fp)
        return fatal_error("Could not open file %s.", filename);  

    int gop_count   = 0;
    int frame_count = 0;

    DCTELEM *prev = NULL;
    DCTELEM *cur  = NULL;
    DCTELEM *next = NULL;
    histogram = CreateSparseHistogram();

    while (!feof(fp)) {
        /* Read raw data from the input file. */
        data_size = fread(input_buffer, 1, INPUT_BUFFER_SIZE, fp);  
        if (!data_size)  
            break;  
		
        /* Use the parser to split the data into frames. */
        data = input_buffer;  
        while (data_size > 0) {   
            bytesParsed = av_parser_parse2(pCodecParserCtx, pCodecCtx,  
                                           &packet.data, &packet.size,  
                                           data, data_size,  
                                           AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);  
            if (bytesParsed < 0)
                return fatal_error("Error while parsing.");
            data      += bytesParsed;  
            data_size -= bytesParsed;

            if (!packet.size)  
                continue;  

            if (pCodecParserCtx->pict_type == AV_PICTURE_TYPE_I) {
                gop_count++;

                int ret = decode_frame(pCodecCtx, pFrame, &packet, frame_count);
                if (ret < 0)
                    return ret;						      
                if (ret > 0) {
	                FREE(prev);
	                prev = cur;
	                cur = next;
	                next = copy_dct_coeff(pFrame->dct_coeff, pCodecCtx->width, pCodecCtx->height);
	                if (cur != NULL) 
	                    ExtractMotionFeatures(prev, cur, next, 
	                                          pCodecCtx->width, pCodecCtx->height, 
	                                          histogram);				
                }
            }
			
            frame_count++;
        }
    }
  
    /* Some codecs, such as MPEG, transmit the I and P frame with a
       latency of one frame. You must do the following to have a
       chance to get the last frame of the video */
    packet.data = NULL;
    packet.size = 0;
    int ret = decode_frame(pCodecCtx, pFrame, &packet, frame_count);
    if (ret < 0)
        return ret;
    if (ret > 0) {
        if (pFrame->pict_type == AV_PICTURE_TYPE_I) {
            FREE(prev);
            prev = cur;
            cur = next;
            next = copy_dct_coeff(pFrame->dct_coeff, pCodecCtx->width, pCodecCtx->height);
            if (cur != NULL) 
                ExtractMotionFeatures(prev, cur, next, 
                                      pCodecCtx->width, pCodecCtx->height, 
                                      histogram);
        }
        frame_count++;
    }
 
    /* Analyze the last frame. */
    if (next != NULL)
        ExtractMotionFeatures(cur, next, NULL, 
                              pCodecCtx->width, pCodecCtx->height, 
                              histogram);
      						
    SparseHistogram2PyArray(npy_arr);

    /* Free the histogram. */
    DestroySparseHistogram(&histogram);

    /* Free the DCT coefficients. */
    FREE(prev);
    FREE(cur);
    FREE(next);

    /* Close input file. */
    fclose(fp);

    /* Close the parser. */
    av_parser_close(pCodecParserCtx);  

    /* Free the frame. */
    avcodec_free_frame(&pFrame);  

    /* Close the codec. */
    avcodec_close(pCodecCtx);

    /* Free the codec. */
    av_free(pCodecCtx);
  
    return 0;  
}  


static PyObject *extract(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

    PyArrayObject *npy_arr = NULL;

    if(parse_video(&npy_arr) < 0) {
        printf("Decoding video failed.\n");

        Py_XDECREF(npy_arr);
        return Py_None;
    }
	
    return (PyObject *) npy_arr;
}


static PyMethodDef HMPMethods[] = {
    {"extract",  extract, METH_VARARGS, "Extract the HMP of a MPEG-2 raw video."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


static struct PyModuleDef hmpmodule = {
    PyModuleDef_HEAD_INIT,
    "hmp",      /* name of module */
    NULL,       /* module documentation, may be NULL */
    -1,         /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    HMPMethods
};


PyMODINIT_FUNC PyInit_hmp(void)
{
    PyObject *m;

    m = PyModule_Create(&hmpmodule);
    if (m == NULL)
        return NULL;

    /* IMPORTANT: this must be called */
    import_array();

    HMPError = PyErr_NewException("hmp.error", NULL, NULL);
    Py_INCREF(HMPError);
    PyModule_AddObject(m, "error", HMPError);
    return m;
}


int main(int argc, char *argv[])
{
    av_log_set_level(AV_LOG_QUIET);

    wchar_t *program = Py_DecodeLocale(argv[0], NULL);
    if (program == NULL) {
        fprintf(stderr, "Error: cannot decode argv[0]\n");
        exit(1);
    }

    /* Add a built-in module, before Py_Initialize */
    PyImport_AppendInittab("hmp", PyInit_hmp);

    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName(program);

    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();

    PyMem_RawFree(program);
    return 0;
}

