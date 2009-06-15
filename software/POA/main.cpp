#include <stdio.h>
#include <stdlib.h>
#include "FreeImage.h"

typedef struct tagMyImage
{
	// image format bmp
	FREE_IMAGE_FORMAT fif;
	// image information
	FIBITMAP *dib;
	BYTE* bits;
	//width and height
	unsigned int width;
	unsigned int height;
} MyImage;

// keep pixel informtion
bool loadMyImage(const char* filename, MyImage *pMyImage)
{

	//image format
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	//pointer to the image, once loaded
	FIBITMAP *dib = NULL;
	//pointer to the image data
	BYTE* bits= NULL;
	//image width and height
	unsigned int width(0), height(0);

	// get image format
	fif = FreeImage_GetFileType(filename, 0);
	//if still unknown, try to guess the file format from the file extension
	if(fif == FIF_UNKNOWN) 
		fif = FreeImage_GetFIFFromFilename(filename);
	//if still unkown, return failure
	if(fif == FIF_UNKNOWN)
		return false;

	//check that the plugin has reading capabilities and load the file
	if(FreeImage_FIFSupportsReading(fif))
		dib = FreeImage_Load(fif, filename);
	//if the image failed to load, return failure
	if(dib==NULL)
		return false;


	bits = FreeImage_GetBits(dib);
	
	width = FreeImage_GetWidth(dib);
	height = FreeImage_GetHeight(dib);
	
	if((bits == 0) || (width == 0) || (height == 0))
		return false;

	pMyImage->fif = fif;
	pMyImage->dib = dib;
	pMyImage->bits = bits;
	pMyImage->width = width;
	pMyImage->height = height;
	pMyImage->bits = bits;

	//return success
	return true;
}

void unloadMyImage(MyImage *pMyImage)
{
	//Free FreeImage's copy of the data
	FreeImage_Unload(pMyImage->dib);
}


/* compare two images, output 3 tiff images*/
int Compare(MyImage image0, MyImage image1, char * loadfile, char * savefile)
{
	RGBQUAD value1;
	RGBQUAD value2;
	RGBQUAD dif;

	// define white pixel
	RGBQUAD white;
	white.rgbGreen = 255;
	white.rgbBlue = 255;
	white.rgbRed= 255;

	// initialize the 3 output image
	MyImage outImage1, outImage2, outImage3;
	loadMyImage(loadfile, &outImage3);
	loadMyImage(loadfile, &outImage1);
	loadMyImage(loadfile, &outImage2);

    unsigned int i,j;
    
	for (j = 0; j < image0.height; ++j)
	{
		for ( i = 0; i < image0.width; ++i)
		{
			// get the pixel of two images
			FreeImage_GetPixelColor(image0.dib, i, j, &value1);
			FreeImage_GetPixelColor(image1.dib, i, j, &value2);

			if (value1.rgbRed != value2.rgbRed ||value1.rgbBlue != value2.rgbBlue||value1.rgbGreen != value2.rgbGreen)
			{
				BYTE r = abs(value1.rgbRed - value2.rgbRed);
			
				BYTE g = abs(value1.rgbGreen - value2.rgbGreen);
			
				BYTE b = abs(value1.rgbBlue - value2.rgbBlue);
			
				
				dif.rgbRed = r;
				dif.rgbGreen = g;
				dif.rgbBlue = b;
			//	dif.rgbReserved = 0;
	
				FreeImage_SetPixelColor(outImage3.dib, i, j, &dif);
				FreeImage_SetPixelColor(outImage1.dib, i, j, &value1);
				FreeImage_SetPixelColor(outImage2.dib, i, j, &value2);
				
			}
			else
			{
				FreeImage_SetPixelColor(outImage3.dib, i, j, &white);
				FreeImage_SetPixelColor(outImage1.dib, i, j, &white);
				FreeImage_SetPixelColor(outImage2.dib, i, j, &white);
			}
		}
	}

	//FreeImage_Save(FIF_TIFF, outImage3.dib, "difference.tif", 0); //FREE_IMAGE_FORMAT::
	//FreeImage_Save(FIF_TIFF, outImage1.dib, "outimage1.tif", 0); 
	FreeImage_Save(FIF_PPM, outImage2.dib, savefile, 0);
	

	return 0;
}

int main()
{
	FreeImage_Initialise(TRUE);
	MyImage image0, image1;
	
	while (1)
	{
	printf("\n================Welcome to POA=================\n");
	printf("To compare application output on one VM with different timestamp, press 1\n");
	printf("To compare application output on different VM, press 2\n");
	printf("To exit, press 0\n");
	
	int choice;
	scanf("%d", &choice);
	switch (choice)
	{
		case 0:
		    return 0;
		case 1: 
		{
	       if (loadMyImage("in1.bmp", &image0) == false)
	       {
		     printf("File in1.bmp does not exist\n");
		     return -1;
     	   }
	
	      if (loadMyImage("in2.bmp", &image1) == false)
	      {
		    printf("File in2.bmp does not exist\n");
		    return -1;
          }
  
          Compare(image0, image1,"in1.bmp","out1.ppm");
          printf("\nThe difference is \n");
          system("gocr out1.ppm");
          
          break;
         }
         
         case 2: 
		{
	       if (loadMyImage("in1.bmp", &image0) == false)
	       {
		     printf("File in1.bmp does not exist\n");
		     return -1;
     	   }
	
	      if (loadMyImage("in2.bmp", &image1) == false)
	      {
		    printf("File in2.bmp does not exist\n");
		    return -1;
          }
          
          Compare(image0, image1,"in1.bmp","out1.ppm");
          system("gocr out1.ppm -o vm1.txt");
          printf("\n difference was generated in vm1.txt\n");
          
          if (loadMyImage("in3.bmp", &image0) == false)
	       {
		     printf("File in3.bmp does not exist\n");
		     return -1;
     	   }
	
	      if (loadMyImage("in4.bmp", &image1) == false)
	      {
		    printf("File in4.bmp does not exist\n");
		    return -1;
          }
           
          Compare(image0, image1,"in3.bmp","out2.ppm");
          system("gocr out2.ppm -o vm2.txt");
          printf("\n difference was generated in vm2.txt\n");
          
          printf("\nThe difference of application output on VM1 and VM2 is \n");
          system("diff vm1.txt vm2.txt");
          
          break;
         }
         
         default:
         {
           printf("incorrect choice\n");
           break;
	     }
	}
   }
	      FreeImage_DeInitialise();

	return 0;
}


