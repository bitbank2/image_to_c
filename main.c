//
// image_to_c - convert binary image files into c-compatible data tables
//
// Written by Larry Bank
// Copyright (c) 2020 BitBank Software, Inc.
// Change history
// 12/2/20 - Started the project
//
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TEMP_BUF_SIZE 4096
#define DEFAULT_READ_SIZE 256
#define MAX_TAGS 256
#define TIFF_TAGSIZE 12

#define INTELSHORT(p) ((*p) + (*(p+1)<<8))
#define INTELLONG(p) ((*p) + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24))
#define MOTOSHORT(p) (((*(p))<<8) + (*(p+1)))
#define MOTOLONG(p) (((*p)<<24) + ((*(p+1))<<16) + ((*(p+2))<<8) + (*(p+3)))

#ifdef _WIN32
#define PILIO_SLASH_CHAR '\\'
#else
#define PILIO_SLASH_CHAR '/'
#endif

typedef unsigned char BOOL;

const char *szType[] = {"Unknown", "PNG","JFIF","Win BMP","OS/2 BMP","TIFF","GIF","Portable Pixmap","Targa","JEDMICS","CALS","PCX"};
const char *szComp[] = {"Unknown", "Flate","JPEG","None","RLE","LZW","G3","G4","Packbits","Modified Huffman","Thunderscan RLE","JBIG (T.85)"};
const char *szPhotometric[] = {"WhiteIsZero","BlackIsZero","RGB","Palette Color","Transparency Mask","CMYK","YCbCr","Unknown"};
const char *szPlanar[] = {"Unknown","Chunky","Planar"};

enum
{
    FILETYPE_UNKNOWN = 0,
    FILETYPE_PNG,
    FILETYPE_JPEG,
    FILETYPE_BMP,
    FILETYPE_OS2BMP,
    FILETYPE_TIFF,
    FILETYPE_GIF,
    FILETYPE_PPM,
    FILETYPE_TARGA,
    FILETYPE_JEDMICS,
    FILETYPE_CALS,
    FILETYPE_PCX
};

enum
{
    COMPTYPE_UNKNOWN = 0,
    COMPTYPE_FLATE,
    COMPTYPE_JPEG,
    COMPTYPE_NONE,
    COMPTYPE_RLE,
    COMPTYPE_LZW,
    COMPTYPE_G3,
    COMPTYPE_G4,
    COMPTYPE_PACKBITS,
    COMPTYPE_HUFFMAN,
    COMPTYPE_THUNDERSCAN,
    COMPTYPE_JBIG
};

FILE * ihandle;
void MakeC(unsigned char *, int, int);
void GetLeafName(char *fname, char *leaf);
void FixName(char *name);

unsigned short TIFFSHORT(unsigned char *p, BOOL bMotorola)
{
    unsigned short s;
    
    if (bMotorola)
        s = *p * 0x100 + *(p+1);
    else
        s = *p + *(p+1)*0x100;
    
    return s;
} /* TIFFSHORT() */

uint32_t TIFFLONG(unsigned char *p, BOOL bMotorola)
{
    uint32_t l;
    
    if (bMotorola)
        l = *p * 0x1000000 + *(p+1) * 0x10000 + *(p+2) * 0x100 + *(p+3);
    else
        l = *p + *(p+1) * 0x100 + *(p+2) * 0x10000 + *(p+3) * 0x1000000;
    
    return l;
} /* TIFFLONG() */

int TIFFVALUE(unsigned char *p, BOOL bMotorola)
{
    int i, iType;
    
    iType = TIFFSHORT(p+2, bMotorola);
    /* If pointer to a list of items, must be a long */
    if (TIFFSHORT(p+4, bMotorola) > 1)
        iType = 4;
    switch (iType)
    {
        case 3: /* Short */
            i = TIFFSHORT(p+8, bMotorola);
            break;
        case 4: /* Long */
        case 7: // undefined (treat it as a long since it's usually a multibyte buffer)
            i = TIFFLONG(p+8, bMotorola);
            break;
        case 6: // signed byte
            i = (signed char)p[8];
            break;
        case 2: /* ASCII */
        case 5: /* Unsigned Rational */
        case 10: /* Signed Rational */
            i = TIFFLONG(p+8, bMotorola);
            break;
        default: /* to suppress compiler warning */
            i = 0;
            break;
    }
    return i;
    
} /* TIFFVALUE() */

int ParseNumber(unsigned char *buf, int *iOff, int iLength)
{
    int i, iOffset;
    
    i = 0;
    iOffset = *iOff;
    
    while (iOffset < iLength && buf[iOffset] >= '0' && buf[iOffset] <= '9')
    {
        i *= 10;
        i += (int)(buf[iOffset++] - '0');
    }
    *iOff = iOffset+1; /* Skip ending char */
    return i;
    
} /* ParseNumber() */

//
// CountGIFFrames
//
// Given a pointer to a multipage GIF image
// This function will walk through the file and count the number
// of frames present
//
int CountGIFFrames(unsigned char *cBuf, int iFileSize)
{
    int iNumFrames;
    size_t iOff;
    int bDone = 0;
    int bExt;
    unsigned char c;
    
    iNumFrames = 0;
    iOff = 10;
    c = cBuf[iOff]; // get info bits
    iOff += 3;   /* Skip flags, background color & aspect ratio */
    if (c & 0x80) /* Deal with global color table */
    {
        c &= 7;  /* Get the number of colors defined */
        iOff += (2<<c)*3; /* skip color table */
    }
    while (!bDone && iOff < iFileSize)
    {
        bExt = 1; /* skip extension blocks */
        while (!bDone && bExt && iOff < iFileSize)
        {
            switch(cBuf[iOff])
            {
                case 0x3b: /* End of file */
                    /* we were fooled into thinking there were more frames */
                    iNumFrames--;
                    bDone = 1;
                    continue;
                    // F9 = Graphic Control Extension (fixed length of 4 bytes)
                    // FE = Comment Extension
                    // FF = Application Extension
                    // 01 = Plain Text Extension
                case 0x21: /* Extension block */
                    iOff += 2; /* skip to length */
                    iOff += (int)cBuf[iOff]; /* Skip the data block */
                    iOff++;
                    // block terminator or optional sub blocks
                    c = cBuf[iOff++]; /* Skip any sub-blocks */
                    while (c && iOff < iFileSize)
                    {
                        iOff += (int)c;
                        c = cBuf[iOff++];
                    }
                    if (c != 0) // problem, we went past the end
                    {
                        iNumFrames--; // possible corrupt data; stop
                        bDone = 1;
                        continue;
                    }
                    break;
                case 0x2c: /* Start of image data */
                    bExt = 0; /* Stop doing extension blocks */
                    break;
                default:
                    /* Corrupt data, stop here */
                    iNumFrames--;
                    bDone = 1;
                    //                    *bTruncated = TRUE;
                    continue;
            }
        }
        if (iOff >= iFileSize) // problem
        {
            iNumFrames--; // possible corrupt data; stop
            bDone = 1;
            //            *bTruncated = TRUE;
            continue;
        }
        /* Start of image data */
        c = cBuf[iOff+9]; /* Get the flags byte */
        iOff += 10; /* Skip image position and size */
        if (c & 0x80) /* Local color table */
        {
            c &= 7;
            iOff += (2<<c)*3;
        }
        iOff++; /* Skip LZW code size byte */
        c = cBuf[iOff++];
        while (c && iOff < iFileSize) /* While there are more data blocks */
        {
            iOff += (int)c;  /* Skip this data block */
            if (iOff > iFileSize) // past end of file, stop
            {
                iNumFrames--; // don't count this frame
                break; // last page is corrupted, don't use it
            }
            c = cBuf[iOff++]; /* Get length of next */
        }
        /* End of image data, check for more frames... */
        if ((iOff > iFileSize) || cBuf[iOff] == 0x3b)
        {
            bDone = 1; /* End of file has been reached */
        }
        else
        {
            iNumFrames++;
        }
    } /* while !bDone */
    return iNumFrames;
    
} /* CountGIFFrames() */

void ImageInfo(FILE *iHandle, int iFileSize, char *szInfo)
{
    int i, j, k;
    int iBytes;
    int iFileType = FILETYPE_UNKNOWN;
    int iCompression = COMPTYPE_UNKNOWN;
    unsigned char cBuf[TEMP_BUF_SIZE]; // small buffer to load header info
    int iBpp = 0;
    int iWidth = 0;
    int iHeight = 0;
    int iOffset;
    int iMarker;
    int iPhotoMetric;
    int iPlanar;
    int iCount;
    unsigned char ucSubSample;
    BOOL bMotorola;
    char szOptions[256];
    
    szInfo[0] = 0; // start with null string
    
    // Detect the file type by its header
    iBytes = fread(cBuf, 1, DEFAULT_READ_SIZE, iHandle);
    if (iBytes != DEFAULT_READ_SIZE)
        return; // too small
    if (MOTOLONG(cBuf) == 0x89504e47) // PNG
        iFileType = FILETYPE_PNG;
    else if (cBuf[0] == 'B' && cBuf[1] == 'M') // BMP
    {
        if (cBuf[14] == 0x28) // Windows
            iFileType = FILETYPE_BMP;
        else
            iFileType = FILETYPE_OS2BMP;
    }
    else if (cBuf[0] == 0x0a && cBuf[1] < 0x6 && cBuf[2] == 0x01)
    {
        iFileType = FILETYPE_PCX;
    }
    else if (INTELLONG(cBuf) == 0x80 && (cBuf[36] == 4 || cBuf[36] == 6))
    {
        iFileType = FILETYPE_JEDMICS;
    }
    else if (INTELLONG(cBuf) == 0x64637273)
    {
        iFileType = FILETYPE_CALS;
    }
    else if ((MOTOLONG(cBuf) & 0xffffff00) == 0xffd8ff00) // JPEG
        iFileType = FILETYPE_JPEG;
    else if (MOTOLONG(cBuf) == 0x47494638 /*'GIF8'*/) // GIF
        iFileType = FILETYPE_GIF;
    else if ((cBuf[0] == 'I' && cBuf[1] == 'I') || (cBuf[0] == 'M' && cBuf[1] == 'M'))
        iFileType = FILETYPE_TIFF;
    else
    {
        i = MOTOLONG(cBuf) & 0xffff8080;
        if (i == 0x50360000 || i == 0x50350000 || i == 0x50340000) // Portable bitmap/graymap/pixmap
            iFileType = FILETYPE_PPM;
    }
    // Check for Truvision Targa
    i = cBuf[1] & 0xfe;
    j = cBuf[2];
    // make sure it is not a MPEG file (starts with 00 00 01 BA)
    if (MOTOLONG(cBuf) != 0x1ba && MOTOLONG(cBuf) != 0x1b3 && i == 0 && (j == 1 || j == 2 || j == 3 || j == 9 || j == 10 || j == 11))
        iFileType = FILETYPE_TARGA;
    
    if (iFileType == FILETYPE_UNKNOWN)
    {
        return;
    }
    szOptions[0] = '\0'; // info specific to each file type
    // Get info specific to each type of file
    switch (iFileType)
    {
        case FILETYPE_PCX:
            iWidth = 1 + INTELSHORT(&cBuf[8]) - INTELSHORT(&cBuf[4]);
            iHeight = 1 + INTELSHORT(&cBuf[10]) - INTELSHORT(&cBuf[6]);
            iCompression = COMPTYPE_PACKBITS;
            iBpp = cBuf[3] * cBuf[65];
            break;
            
        case FILETYPE_PNG:
            if (MOTOLONG(&cBuf[12]) == 0x49484452/*'IHDR'*/)
            {
                iWidth = MOTOLONG(&cBuf[16]);
                iHeight = MOTOLONG(&cBuf[20]);
                iCompression = COMPTYPE_FLATE;
                i = cBuf[24]; // bits per pixel
                j = cBuf[25]; // pixel type
                switch (j)
                {
                    case 0: // grayscale
                    case 3: // palette image
                        iBpp = i;
                        break;
                    case 2: // RGB triple
                        iBpp = i * 3;
                        break;
                    case 4: // grayscale + alpha channel
                        iBpp = i * 2;
                        break;
                    case 6: // RGB + alpha
                        iBpp = i * 4;
                        break;
                }
                if (cBuf[28] == 1) // interlace flag
                    strcpy(szOptions, ", Interlaced");
                else
                    strcpy(szOptions, ", Not interlaced");
            }
            break;
        case FILETYPE_TARGA:
            iWidth = INTELSHORT(&cBuf[12]);
            iHeight = INTELSHORT(&cBuf[14]);
            iBpp = cBuf[16];
            if (cBuf[2] == 3 || cBuf[2] == 11) // monochrome
                iBpp = 1;
            if (cBuf[2] < 9)
                iCompression = COMPTYPE_NONE;
            else
                iCompression = COMPTYPE_RLE;
            break;
        case FILETYPE_PPM:
            if (cBuf[1] == '4')
                iBpp = 1;
            else if (cBuf[1] == '5')
                iBpp = 8;
            else if (cBuf[1] == '6')
                iBpp = 24;
            j = 2;
            while ((cBuf[j] == 0xa || cBuf[j] == 0xd) && j<DEFAULT_READ_SIZE)
                j++; // skip newline/cr
            while (cBuf[j] == '#' && j < DEFAULT_READ_SIZE) // skip over comments
            {
                while (cBuf[j] != 0xa && cBuf[j] != 0xd && j < DEFAULT_READ_SIZE)
                    j++;
                while ((cBuf[j] == 0xa || cBuf[j] == 0xd) && j<DEFAULT_READ_SIZE)
                    j++; // skip newline/cr
            }
            // get width and height
            iWidth = ParseNumber(cBuf, &j, DEFAULT_READ_SIZE);
            iHeight = ParseNumber(cBuf, &j, DEFAULT_READ_SIZE);
            iCompression = COMPTYPE_NONE;
            break;
        case FILETYPE_BMP:
            iCompression = COMPTYPE_NONE;
            iWidth = INTELSHORT(&cBuf[18]);
            iHeight = INTELSHORT(&cBuf[22]);
            if (iHeight & 0x8000) // upside down
                iHeight = 65536 - iHeight;
            iBpp = cBuf[28]; /* Number of bits per plane */
            iBpp *= cBuf[26]; /* Number of planes */
            if (cBuf[30] && (iBpp == 4 || iBpp == 8)) // if biCompression is non-zero (2=4bit rle, 1=8bit rle,4=24bit rle)
                iCompression = COMPTYPE_RLE; // windows run-length
            break;
        case FILETYPE_OS2BMP:
            iCompression = COMPTYPE_NONE;
            if (cBuf[14] == 12) // version 1.2
            {
                iWidth = INTELSHORT(&cBuf[18]);
                iHeight = INTELSHORT(&cBuf[20]);
                iBpp = cBuf[22]; /* Number of bits per plane */
                iBpp *= cBuf[24]; /* Number of planes */
            }
            else
            {
                iWidth = INTELSHORT(&cBuf[18]);
                iHeight = INTELSHORT(&cBuf[22]);
                iBpp = cBuf[28]; /* Number of bits per plane */
                iBpp *= cBuf[26]; /* Number of planes */
            }
            if (iHeight & 0x8000) // upside down
                iHeight = 65536 - iHeight;
            if (cBuf[30] == 1 || cBuf[30] == 2 || cBuf[30] == 4) // if biCompression is non-zero (2=4bit rle, 1=8bit rle,4=24bit rle)
                iCompression = COMPTYPE_RLE; // windows run-length
            break;
        case FILETYPE_JEDMICS:
            iBpp = 1;
            iWidth = INTELSHORT(&cBuf[6]);
            iWidth <<= 3; // convert byte width to pixel width
            iHeight = INTELSHORT(&cBuf[4]);
            iCompression = COMPTYPE_G4;
            break;
        case FILETYPE_CALS:
            iBpp = 1;
            iCompression = COMPTYPE_G4;
            fseek(iHandle, 750, SEEK_SET); // read some more
            iBytes = fread(cBuf, 1, 1, iHandle);
            if (cBuf[0] == '1') // type 1 file
            {
                fseek(iHandle, 1033, SEEK_SET); // read some more
                iBytes = fread(cBuf, 1, 256, iHandle);
                i = 0;
                iWidth = ParseNumber(cBuf, &i, 256);
                iHeight = ParseNumber(cBuf, &i, 256);
            }
            else // type 2
            {
                fseek(iHandle, 1024, SEEK_SET); // read some more
                iBytes = fread(cBuf, 1, 128, iHandle);
                if (MOTOLONG(cBuf) == 0x7270656c && MOTOLONG(&cBuf[4]) == 0x636e743a) // "rpelcnt:"
                {
                    i = 9;
                    iWidth = ParseNumber(cBuf, &i, 256);
                    iHeight = ParseNumber(cBuf, &i, 256);
                }
            }
            break;
        case FILETYPE_JPEG:
            iCompression = COMPTYPE_JPEG;
            i = j = 2; /* Start at offset of first marker */
            iMarker = 0; /* Search for SOF (start of frame) marker */
            while (i < 32 && iMarker != 0xffc0 && j < iFileSize)
            {
                iMarker = MOTOSHORT(&cBuf[i]) & 0xfffc;
                if (iMarker < 0xff00) // invalid marker, could be generated by "Arles Image Web Page Creator" or Accusoft
                {
                    i += 2;
                    continue; // skip 2 bytes and try to resync
                }
                if (iMarker == 0xffe0 && cBuf[i+4] == 'E' && cBuf[i+5] == 'x') // EXIF, check for thumbnail
                {
                    unsigned char cTemp[1024];
                    //               int iOff;
                    memcpy(cTemp, cBuf, 32);
                    iBytes = fread(&cTemp[32], 1, 1024-32, iHandle);
                    bMotorola = (cTemp[i+10] == 'M');
                    // Future - do something with the thumbnail
                    //               iOff = PILTIFFLONG(&cTemp[i+14], bMotorola); // get offset to first IFD (info)
                    //               PILTIFFMiniInfo(pFile, bMotorola, j + 10 + iOff, TRUE);
                }
                if (iMarker == 0xffc0) // the one we're looking for
                    break;
                j += 2 + MOTOSHORT(&cBuf[i+2]); /* Skip to next marker */
                if (j < iFileSize) // need to read more
                {
                    fseek(iHandle, j, SEEK_SET); // read some more
                    iBytes = fread(cBuf, 1, 32, iHandle);
                    i = 0;
                }
            } // while
            if (iMarker != 0xffc0)
                return; // error - invalid file?
            else
            {
                iBpp = cBuf[i+4]; // bits per sample
                iHeight = MOTOSHORT(&cBuf[i+5]);
                iWidth = MOTOSHORT(&cBuf[i+7]);
                iBpp = iBpp * cBuf[i+9]; /* Bpp = number of components * bits per sample */
                ucSubSample = cBuf[i+11];
                sprintf(szOptions, ", color subsampling = %d:%d", (ucSubSample>>4),(ucSubSample & 0xf));
            }
            break;
        case FILETYPE_GIF:
            iCompression = COMPTYPE_LZW;
            iWidth = INTELSHORT(&cBuf[6]);
            iHeight = INTELSHORT(&cBuf[8]);
            iBpp = (cBuf[10] & 7) + 1;
            if (cBuf[10] & 64) // interlace flag
                strcpy(szOptions, ", Interlaced");
            else
                strcpy(szOptions, ", Not interlaced");
            break;
        case FILETYPE_TIFF:
            bMotorola = (cBuf[0] == 'M'); // determine endianness of TIFF data
            i = TIFFLONG(&cBuf[4], bMotorola); // get first IFD offset
            fseek(iHandle, i, SEEK_SET);// read the entire tag directory
            iBytes = fread(cBuf, 1, MAX_TAGS*TIFF_TAGSIZE, iHandle);
            j = TIFFSHORT(cBuf, bMotorola); // get the tag count
            iOffset = 2; // point to start of TIFF tag directory
            // Some TIFF files don't specify everything, so set up some default values
            iBpp = 1;
            iPlanar = 1;
            iCompression = COMPTYPE_NONE;
            iPhotoMetric = 7; // if not specified, set to "unknown"
            // Each TIFF tag is made up of 12 bytes
            // byte 0-1: Tag value (short)
            // byte 2-3: data type (short)
            // byte 4-7: number of values (long)
            // byte 8-11: value or offset to list of values
            for (i=0; i<j; i++) // search tags for the info we care about
        {
            iMarker = TIFFSHORT(&cBuf[iOffset], bMotorola); // get the TIFF tag
            switch (iMarker) // only read the tags we care about...
            {
                case 256: // image width
                    iWidth = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    break;
                case 257: // image length
                    iHeight = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    break;
                case 258: // bits per sample
                    iCount = TIFFLONG(&cBuf[iOffset+4], bMotorola); /* Get the count */
                    if (iCount == 1)
                        iBpp = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    else // need to read the first value from the list (they should all be equal)
                    {
                        k = TIFFLONG(&cBuf[iOffset+8], bMotorola);
                        if (k < iFileSize)
                        {
                            fseek(iHandle, k, SEEK_SET);
                            iBytes = fread(&cBuf[iOffset], 1, 2, iHandle); // okay to overwrite the value we just used
                            iBpp = iCount * TIFFSHORT(&cBuf[iOffset], bMotorola);
                        }
                    }
                    break;
                case 259: // compression
                    k = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    if (k == 1)
                        iCompression = COMPTYPE_NONE;
                    else if (k == 2)
                        iCompression = COMPTYPE_HUFFMAN;
                    else if (k == 3)
                        iCompression = COMPTYPE_G3;
                    else if (k == 4)
                        iCompression = COMPTYPE_G4;
                    else if (k == 5)
                        iCompression = COMPTYPE_LZW;
                    else if (k == 6 || k == 7)
                        iCompression = COMPTYPE_JPEG;
                    else if (k == 8 || k == 32946)
                        iCompression = COMPTYPE_FLATE;
                    else if (k == 9)
                        iCompression = COMPTYPE_JBIG;
                    else if (k == 32773)
                        iCompression = COMPTYPE_PACKBITS;
                    else if (k == 32809)
                        iCompression = COMPTYPE_THUNDERSCAN;
                    else
                        iCompression = COMPTYPE_UNKNOWN;
                    break;
                case 262: // photometric value
                    iPhotoMetric = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    if (iPhotoMetric > 6)
                        iPhotoMetric = 7; // unknown
                    break;
                case 284: // planar/chunky
                    iPlanar = TIFFVALUE(&cBuf[iOffset], bMotorola);
                    if (iPlanar < 1 || iPlanar > 2) // unknown value
                        iPlanar = 0; // unknown
                    break;
            } // switch on tiff tag
            iOffset += TIFF_TAGSIZE;
        } // for each tag
//            sprintf(szOptions, ", Photometric = %s, Planar config = %s", szPhotometric[iPhotoMetric], szPlanar[iPlanar]);
//            break;
    } // switch
    sprintf(szInfo, "// %s, Compression=%s, Size: %d x %d, %d-Bpp\n", szType[iFileType], szComp[iCompression], iWidth, iHeight, iBpp);
    if (iFileType == FILETYPE_GIF) // see how many frames it has
    {
        // slight hack - load the file into memory
        uint8_t *pFile;
        char szTemp[32];
        int iFrames;
        pFile = malloc(iFileSize);
        if (pFile != NULL)
        {
            fseek(iHandle, 0, SEEK_SET);
            fread(pFile, 1, iFileSize, iHandle);
            iFrames = CountGIFFrames(pFile, iFileSize);
            free(pFile);
            sprintf(szTemp, "// %d frames\n//\n", iFrames);
            strcat(szInfo, szTemp);
        }
    }
    else
    {
        strcat(szInfo, "//\n"); // simple end of comment
    }
} /* ImageInfo() */
//
// Main program entry point
//
int main(int argc, char *argv[])
{
    int iSize, iData;
    unsigned char *p;
    char szLeaf[256];
    char szInfo[256];
    
    if (argc != 2)
    {
        printf("image_to_c Copyright (c) 2020 BitBank Software, Inc.\n");
        printf("Written by Larry Bank\n\n");
        printf("Usage: image_to_c <filename>\n");
        printf("output is written to stdout\n");
        printf("example:\n\n");
        printf("image_to_c ./test.jpg > test.h\n");
        return 0; // no filename passed
    }
    ihandle = fopen(argv[1],"rb"); // open input file
    if (ihandle == NULL)
    {
        fprintf(stderr, "Unable to open file: %s\n", argv[1]);
        return -1; // bad filename passed
    }
    
    fseek(ihandle, 0L, SEEK_END); // get the file size
    iSize = (int)ftell(ihandle);
    fseek(ihandle, 0, SEEK_SET);
    ImageInfo(ihandle, iSize, szInfo); // get image info
    p = (unsigned char *)malloc(0x10000); // allocate 64k to play with
    GetLeafName(argv[1], szLeaf);
    printf("// Created with image_to_c\n// https://github.com/bitbank2/image_to_c\n");
    printf("//\n// %s\n// Data size = %d bytes\n//\n", szLeaf, iSize); // comment header with filename
    if (szInfo[0])
        printf("%s", szInfo);
    FixName(szLeaf); // remove unusable characters
    printf("// for non-Arduino builds...\n");
    printf("#ifndef PROGMEM\n#define PROGMEM\n#endif\n");
    printf("const uint8_t %s[] PROGMEM = {\n", szLeaf); // start of data array
    fseek(ihandle, 0, SEEK_SET);
    while (iSize)
    {
        iData = fread(p, 1, 0x10000, ihandle); // try to read 64k
        MakeC(p, iData, iSize == iData); // create the output data
        iSize -= iData;
    }
    free(p);
    fclose(ihandle);
    printf("};\n"); // final closing brace
    return 0;
} /* main() */
//
// Generate C hex characters from each byte of file data
//
void MakeC(unsigned char *p, int iLen, int bLast)
{
    int i, j, iCount;
    char szTemp[256], szOut[256];
    
    iCount = 0;
    for (i=0; i<iLen>>4; i++) // do lines of 16 bytes
    {
        strcpy(szOut, "\t");
        for (j=0; j<16; j++)
        {
            if (iCount == iLen-1 && bLast) // last one, skip the comma
                sprintf(szTemp, "0x%02x", p[(i*16)+j]);
            else
                sprintf(szTemp, "0x%02x,", p[(i*16)+j]);
            strcat(szOut, szTemp);
            iCount++;
        }
        if (!bLast || iCount != iLen)
            strcat(szOut, "\n");
        printf("%s",szOut);
    }
    p += (iLen & 0xfff0); // point to last section
    if (iLen & 0xf) // any remaining characters?
    {
        strcpy(szOut, "\t");
        for (j=0; j<(iLen & 0xf); j++)
        {
            if (iCount == iLen-1 && bLast)
                sprintf(szTemp, "0x%02x", p[j]);
            else
                sprintf(szTemp, "0x%02x,", p[j]);
            strcat(szOut, szTemp);
            iCount++;
        }
        if (!bLast)
            strcat(szOut, "\n");
        printf("%s",szOut);
    }
} /* MakeC() */
//
// Make sure the name can be used in C/C++ as a variable
// replace invalid characters and make sure it starts with a letter
//
void FixName(char *name)
{
    char c, *d, *s, szTemp[256];
    int i, iLen;
    
    iLen = strlen(name);
    d = szTemp;
    s = name;
    if (s[0] >= '0' && s[0] <= '9') // starts with a digit
        *d++ = '_'; // Insert an underscore
    for (i=0; i<iLen; i++)
    {
        c = *s++;
        // these characters can't be in a variable name
        if (c < ' ' || (c >= '!' && c < '0') || (c > 'Z' && c < 'a'))
            c = '_'; // convert all to an underscore
        *d++ = c;
    }
    *d++ = 0;
    strcpy(name, szTemp);
} /* FixName() */
//
// Trim off the leaf name from a fully
// formed file pathname
//
void GetLeafName(char *fname, char *leaf)
{
    int i, iLen;
    
    iLen = strlen(fname);
    for (i=iLen-1; i>=0; i--)
    {
        if (fname[i] == '\\' || fname[i] == '/') // Windows or Linux
            break;
    }
    strcpy(leaf, &fname[i+1]);
    // remove the filename extension
    iLen = strlen(leaf);
    for (i=iLen-1; i>=0; i--)
    {
        if (leaf[i] == '.')
        {
            leaf[i] = 0;
            break;
        }
    }
} /* GetLeafName() */

