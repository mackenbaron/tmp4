#pragma once
#include<stdio.h>
extern "C"
{
#include "libavformat\avformat.h"
};

#include <string>
#include <vector>

struct Config{
	Config(){}
	~Config(){}

	void MakeFileName()
	{
		std::string::size_type found = _inputFile.find_last_of("/\\");
		_inputFileName = _inputFile.substr(found + 1);
	}

	std::string _inputFile;
	std::string _inputFileName;
	std::string _outputPath;
	std::string _outputFile;
};



typedef struct nal_s{
	unsigned char type;
	bool bHasStartCode;
	int pos;
	int size;
	std::vector<nal_s> vecSons;
}nalu_s,pktu_s;

#define USE_H264BSF 0
#define USE_AACBSF 0
#define IO_BUFFER_SIZE 32768  

class mp4Writer
{
public:
	mp4Writer();
	~mp4Writer();

	int writeMp4(Config &cfg);

private:
	int init();
	int cleanup();
	int nal_parser(AVPacket *org);
	int nal_parser_sub(unsigned char *pNal, nal_s &node);
	bool is_delete(unsigned char type);
	int get_data_size(unsigned char *pHead);
	int set_data_size(unsigned char *pHead, int size);

private:
	AVInputFormat *ifmt_v;
	AVInputFormat *ifmt_a;
	AVOutputFormat *ofmt;
	AVFormatContext *ifmt_ctx_v;
	AVFormatContext *ifmt_ctx_a;
	AVFormatContext *ofmt_ctx;

	int videoindex_v;
	int videoindex_out;
	int audioindex_a;
	int audioindex_out;

	std::string in_filename_v;
	std::string in_filename_a;
	std::string out_filename;
	int bHasAudio;

	unsigned char *_buffer;
};

