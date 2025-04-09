// IEC 18004 (QR) encoding
//

enum {                          // Error Correction Level
   QR_ECL_L = 'L',              // 01
   QR_ECL_M = 'M',              // 00
   QR_ECL_Q = 'Q',              // 11
   QR_ECL_H = 'H',              // 10
};

#define	QR_TAG_BLACK	1       // Value is black
#define	QR_TAG_FIXED	2       // Value is fixed (unfixed is data or control)
#define	QR_TAG_DATA	4       // Value is data of some sort (including ECC and padding)
#define	QR_TAG_ECC	8       // Value is ECC data
#define	QR_TAG_PAD	16      // Value is padding data
#define	QR_TAG_TARGET	32      // Value is target
#define	QR_TAG_ALIGN	64      // Value is alignment or timing
#define	QR_TAG_SET	128     // Value is defined (otherwise is quiet)

typedef struct {
   unsigned int len;            // Data length
   const char *data;            // Data (can include nulls if required)
   unsigned char ver;           // Size of QR code (units) 1-40, or 0 for auto
   char ecl;                    // Error correction level L/M/Q/H, 0 is L (or better if same size code)
   char mask;                   // Masking code '0'-'7'
   const char *mode;            // Character mode string (characters N, A, 8, K, though K is not supported yet)
   unsigned int *widthp;        // Return width
   unsigned int eci;            // Coding indicator (0=auto)
   unsigned char fnc1;          // Function code 0-2
   unsigned char sam;           // Structured append 1-16 index
   unsigned char san;           // Structured append 1-16 total
   unsigned char parity;        // Structured append Parity (XOR of all data in total barcode, so same for all parts)
   unsigned char ai;            // Application Indicator (FNC1=2)
   unsigned char noquiet:1;     // No quiet space included
   unsigned char rotate:2;      // Rotate image
   unsigned int padlen;         // Length of raw padding data to use
   const unsigned char *pad;    // Raw padding data to use. First byte is always for partial byte, and final byte for stray bits in code
   unsigned int minsize;        // Minimum size (goes for bigger ver if needed, even if ver specified smaller)
   unsigned char *verp;         // Return version used
   char *eclp;                  // Return ecl used
   char *maskp;                 // Return mask used
   char **modep;                // Return mode string
   unsigned int *padlenp;       // Return number of pad bytes
   short **padmap;              // Return malloc'd map of bits for padding
   unsigned int *scorep;        // Return score (for mask selection, as per 7.8.3.1)
} qr_encode_t;
#define	qr_encode(...)	qr_encode_opts((qr_encode_t){__VA_ARGS__})
unsigned char *qr_encode_opts(qr_encode_t);

// Used internally, but this allows you to see what the automatic mode calculation comes up with
void qr_mode(char *mode, unsigned char ver, unsigned int len, const char *input);       // Work out a mode for input
void qr_padding(unsigned int len, unsigned char *pad);  // Fill in standard padding
