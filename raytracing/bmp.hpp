class BmpImage
{
private:
	unsigned char * image;
	int width, height;

	const static int sizeoffileheader = 14;
	const static int sizeofbitmapheader = 40;
	const static int sizeofheader = sizeoffileheader + sizeofbitmapheader;

	inline unsigned char toByte( double value ) {
		if (value >= 1.0)
			return (unsigned char) 255;
		else if (value <= 0.0)
			return (unsigned char) 0;
		else
			return (unsigned char) (value * 255.0);
	}

public:
	BmpImage(int width, int height) : width(width), height(height) {
		image = (unsigned char *) malloc (3*width*height*sizeof(unsigned char));
	}
	
	~BmpImage() {
		free(image);
	}
	
	inline void setPixel(int row, int col, double r, double g, double b) {
		int pos = row*3*width+3*col;
		image[pos+2] = toByte(r);	// RGB values are kept in reverse order
		image[pos+1] = toByte(g);
		image[pos+0] = toByte(b);
	}

	inline void setPixelChar(int row, int col, char r, char g, char b) {
		int pos = row*3*width+3*col;
		image[pos+2] = (r);	// RGB values are kept in reverse order
		image[pos+1] = (g);
		image[pos+0] = (b);
	}

	void writeBmpFile(const char * filename) {
		int padsize = (4 - (width*3) % 4) % 4;
		int filesize = sizeofheader + height * (3*width + padsize);
		
		unsigned char fileheader[sizeoffileheader] = {'B','M', 0,0,0,0, 0,0, 0,0, sizeofheader,0,0,0};
		unsigned char bitmapheader[sizeofbitmapheader] = {sizeofbitmapheader,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
		unsigned char pad[] = {0,0,0};
		
		fileheader[ 2] = (unsigned char) (filesize >> 0);
		fileheader[ 3] = (unsigned char) (filesize >> 8);
		fileheader[ 4] = (unsigned char) (filesize >> 16);
		fileheader[ 5] = (unsigned char) (filesize >> 24);
		
		bitmapheader[ 4] = (unsigned char) (width >> 0);
		bitmapheader[ 5] = (unsigned char) (width >> 8);
		bitmapheader[ 6] = (unsigned char) (width >> 16);
		bitmapheader[ 7] = (unsigned char) (width >> 24);
		bitmapheader[ 8] = (unsigned char) (height >> 0);
		bitmapheader[ 9] = (unsigned char) (height >> 8);
		bitmapheader[10] = (unsigned char) (height >> 16);
		bitmapheader[11] = (unsigned char) (height >> 24);
		
		FILE * f = fopen(filename, "wb");
		
		fwrite(fileheader, sizeof(unsigned char), sizeoffileheader, f);
		fwrite(bitmapheader, sizeof(unsigned char), sizeofbitmapheader, f);
		
		for (int row=0; row<height; row++)
		{
			fwrite(&image[row*3*width], sizeof(unsigned char), 3*width, f);
			fwrite(pad, sizeof(unsigned char), padsize, f);
		}
		
		fclose(f);
	}
};
