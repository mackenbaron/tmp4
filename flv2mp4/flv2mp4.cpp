
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};

#include <iostream>
#include <string>
#include "mp4Writer.h"

using namespace std;

struct MConfig{

	MConfig()
	{
		videoIndex = -1;
		audioIndex = -1;
		filename = "4_mudu1.flv";
	}

	int videoIndex;
	int audioIndex;
	string filename;
};

int main(int argc, char *argv[])
{
	Config cfg;
	
	if (argc == 1){
		printf("version : 0.1.5\n");
		printf("usage : flv2mp4 sample.flv sample.mp4\n");
		return 0;
	}

	if (argc!=3)
	{
		printf("input param error\n");
		return -2;
	}

	cfg._inputFile = argv[1];
	cfg._outputFile = argv[2];

	mp4Writer mw;
	int ret = mw.writeMp4(cfg);

	return ret;
}
