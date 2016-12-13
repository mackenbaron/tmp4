#include "mp4Writer.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <list>

FILE *fp_open_v;
FILE *fp_open_a;

mp4Writer::mp4Writer()
{
	_inVideoCfg.ifmt = NULL;
	_inAudioCfg.ifmt = NULL;
	_outputCfg.ofmt = NULL;
	_inVideoCfg.ifmt_ctx = NULL;
	_inAudioCfg.ifmt_ctx = NULL;
	_outputCfg.ofmt_ctx = NULL;

	_inVideoCfg.index = -1;
	_outputCfg.indexVideo = -1;
	_inAudioCfg.index = -1;
	_outputCfg.indexAudio = -1;

	_buffer = new unsigned char[2*1024 * 1024];
}


mp4Writer::~mp4Writer()
{
	delete _buffer;
}

static int fill_iobuffer_v(void *opaque, uint8_t *buf, int buf_size)
{
	if (!feof(fp_open_v)){
		int true_size = fread(buf, 1, buf_size, fp_open_v);
		return true_size;
	}
	else{
		return -1;
	}
}

static int fill_iobuffer_a(void *opaque, uint8_t *buf, int buf_size)
{
	if (!feof(fp_open_a)){
		int true_size = fread(buf, 1, buf_size, fp_open_a);
		return true_size;
	}
	else{
		return -1;
	}
}

int mp4Writer::open_input_file(InputCfg_S *p, FILE * &pFile, char *url,
	int(*read_packet)(void *opaque, uint8_t *buf, int buf_size))
{
	int ret;
	unsigned char *iobuffer_v;
	AVIOContext *avio_v;

	pFile = fopen(p->filename.c_str(), "rb");
	if (pFile == NULL)
	{
		fprintf(stderr, "[failed] Could not open input file\n");
		ret = ErrorNo_FileOpenFail;
		goto end;
	}
	p->ifmt_ctx = avformat_alloc_context();
	iobuffer_v = (unsigned char *)av_malloc(IO_BUFFER_SIZE);
	avio_v = avio_alloc_context(iobuffer_v, IO_BUFFER_SIZE, 0, NULL, read_packet, NULL, NULL);
	p->ifmt_ctx->pb = avio_v;

	p->ifmt = av_find_input_format("flv");
	if ((ret = avformat_open_input(&p->ifmt_ctx, url, p->ifmt, NULL)) < 0) {
		fprintf(stderr, "[failed] Could not open input file\n");
		ret = ErrorNo_FileOpenFail;
		goto end;
	}

	if ((ret = avformat_find_stream_info(p->ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "[failed] Failed to retrieve input stream information\n");
		ret = ErrorNo_Unknow;
		goto end;
	}

	return 0;
end:
	if (pFile){
		fclose(pFile);
		pFile = NULL;
	}
	if (p->ifmt_ctx){
		avformat_close_input(&p->ifmt_ctx);
		p->ifmt_ctx = NULL;
	}
	return ret;
}

int mp4Writer::bind_stream(InputCfg_S *in, OutputCfg_S *out, int type, int &outIndex)
{
	int ret;
	for (int i = 0; i < in->ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if (in->ifmt_ctx->streams[i]->codec->codec_type == type){
			AVStream *in_stream = in->ifmt_ctx->streams[i];
			AVStream *out_stream = avformat_new_stream(out->ofmt_ctx, in_stream->codec->codec);
			in->index = i;
			if (!out_stream) {
				fprintf(stderr, "[failed] Failed allocating output stream\n");

				ret = ErrorNo_Unknow;
				goto end;
			}
			outIndex = out_stream->index;
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf("Failed to copy context from input to output stream codec context\n");
				ret = ErrorNo_Unknow;
				goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (out->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}
	}

	return 0;
end:
	return ret;
}

int mp4Writer::open_output_file(OutputCfg_S *p, InputCfg_S *inV, InputCfg_S *inA)
{
	int ret;

	avformat_alloc_output_context2(&p->ofmt_ctx, NULL, NULL, p->filename.c_str());
	if (!p->ofmt_ctx) {
		fprintf(stderr, "[failed] Could not create output context\n");

		ret = ErrorNo_Unknow;
		goto end;
	}
	p->ofmt = p->ofmt_ctx->oformat;

	ret = bind_stream(inV, p, AVMEDIA_TYPE_VIDEO, p->indexVideo);
	ret |= bind_stream(inA, p, AVMEDIA_TYPE_AUDIO, p->indexAudio);
	if (ret != 0)
	{
		goto end;
	}

	if (inV->index == -1 || inA->index == -1 || p->indexAudio == -1 || p->indexVideo == -1){
		ret = ErrorNo_NoVideoOrAudio;
		if (inV->index == -1)
			fprintf(stderr, "[failed] no video stream\n");
		if (inA->index == -1)
			fprintf(stderr, "[failed] no audio stream\n");
		goto end;
	}

	//Open output file
	if (!(p->ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&p->ofmt_ctx->pb, p->filename.c_str(), AVIO_FLAG_WRITE) < 0) {
			printf("Could not open output file '%s'", p->filename.c_str());
			ret = ErrorNo_FileOpenFail;
			goto end;
		}
	}

	return 0;
end:
	if (p->ofmt_ctx)
	{
		if (p->ofmt_ctx && !(p->ofmt->flags & AVFMT_NOFILE))
			avio_close(p->ofmt_ctx->pb);
		avformat_free_context(p->ofmt_ctx);
		p->ofmt_ctx = NULL;
	}
	return ret;
}

int mp4Writer::init()
{
	int ret;
	//FIX
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

#if USE_AACBSF
	AVBitStreamFilterContext* aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif

	av_register_all();

	ret = open_input_file(&_inVideoCfg, fp_open_v, "", fill_iobuffer_v);
	if (ret != 0){
		goto end;
	}
	ret = open_input_file(&_inAudioCfg, fp_open_a, "nothing", fill_iobuffer_a);
	if (ret != 0){
		goto end;
	}
	ret = open_output_file(&_outputCfg, &_inVideoCfg, &_inAudioCfg);
	if (ret != 0){
		goto end;
	}
	

	return 0;
end:
	return ret;
}

int mp4Writer::cleanup()
{
	if (_inVideoCfg.ifmt_ctx){
		avformat_close_input(&_inVideoCfg.ifmt_ctx);
	}
	if (_inAudioCfg.ifmt_ctx){
		avformat_close_input(&_inAudioCfg.ifmt_ctx);
	}
	/* close output */
	if (_outputCfg.ofmt_ctx && !(_outputCfg.ofmt->flags & AVFMT_NOFILE))
		avio_close(_outputCfg.ofmt_ctx->pb);
	if (_outputCfg.ofmt_ctx){
		avformat_free_context(_outputCfg.ofmt_ctx);
	}

	return 0;
}

int mp4Writer::writeMp4(Config &cfg)
{
	int ret = 0;
	_inVideoCfg.filename = cfg._inputFile;
	_inAudioCfg.filename = cfg._inputFile;
	_outputCfg.filename = cfg._outputFile;

	ret = init();
	if (ret != 0){
		cleanup();
		return ret;
	}

	int64_t cur_pts_v = 0;
	int64_t cur_pts_a = 0;
	int frame_index = 0;
	AVPacket pkt;

	//Write file header
	if (avformat_write_header(_outputCfg.ofmt_ctx, NULL) < 0) {
		fprintf(stderr, "[failed] Error occurred when opening output file\n");
		ret = ErrorNo_Unknow;
		goto end;
	}

	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index = 0;
		AVStream *in_stream, *out_stream;

		//Get an AVPacket
		{
			if (av_compare_ts(cur_pts_v, _inVideoCfg.ifmt_ctx->streams[_inVideoCfg.index]->time_base, cur_pts_a, _inAudioCfg.ifmt_ctx->streams[_inAudioCfg.index]->time_base) <= 0){
				ifmt_ctx = _inVideoCfg.ifmt_ctx;
				stream_index = _outputCfg.indexVideo;

				if (av_read_frame(ifmt_ctx, &pkt) >= 0){
					do{
						in_stream = ifmt_ctx->streams[pkt.stream_index];
						out_stream = _outputCfg.ofmt_ctx->streams[stream_index];

						if (pkt.stream_index == _inVideoCfg.index){
							//FIX：No PTS (Example: Raw H.264)
							//Simple Write PTS
							if (pkt.pts == AV_NOPTS_VALUE){
								//Write PTS
								AVRational time_base1 = in_stream->time_base;
								//Duration between 2 frames (us)
								int64_t calc_duration = (double)AV_TIME_BASE / 16;// av_q2d(in_stream->r_frame_rate);
								//Parameters
								pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
								pkt.dts = pkt.pts;
								pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
								frame_index++;
							}

							cur_pts_v = pkt.pts;

							nal_parser(&pkt);

							break;
						}
					} while (av_read_frame(ifmt_ctx, &pkt) >= 0);
				}
				else{
					break;
				}
			}
			else{
				ifmt_ctx = _inAudioCfg.ifmt_ctx;
				stream_index = _outputCfg.indexAudio;
				if (av_read_frame(ifmt_ctx, &pkt) >= 0){
					do{
						in_stream = ifmt_ctx->streams[pkt.stream_index];
						out_stream = _outputCfg.ofmt_ctx->streams[stream_index];
						if (pkt.stream_index == _inAudioCfg.index){
							//FIX：No PTS
							//Simple Write PTS
							if (pkt.pts == AV_NOPTS_VALUE){
								//Write PTS
								AVRational time_base1 = in_stream->time_base;
								//Duration between 2 frames (us)
								int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
								//Parameters
								pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
								pkt.dts = pkt.pts;
								pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
								frame_index++;
							}
							cur_pts_a = pkt.pts;
							break;
						}
					} while (av_read_frame(ifmt_ctx, &pkt) >= 0);
				}
				else{
					break;
				}
			}
		}

		//FIX:Bitstream Filter
#if USE_H264BSF
		av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if  USE_AACBSF
		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index = stream_index;

		//printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
		//Write
		if (av_interleaved_write_frame(_outputCfg.ofmt_ctx, &pkt) < 0) {
			fprintf(stderr, "[failed] Error muxing packet\n");
			//break;
		}

		av_free_packet(&pkt);
	}
	//Write file trailer
	av_write_trailer(_outputCfg.ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

	return 0;
end:
	cleanup();
	return ret;
}

bool mp4Writer::is_delete(unsigned char type)
{
	static int ppsFlag = 0;
	static int spsFlag = 0;
	static int seiFlag = 0;

	switch (type)
	{
	case 6:
	{
		if (seiFlag == 0){
			seiFlag = 1;
			return false;
		}
		else{
			return true;
		}
	}
		break;
	case 7:
		if (spsFlag == 0){
			spsFlag = 1;
			return false;
		}
		else{
			return true;
		}
		break;
	case 8:
		if (ppsFlag == 0){
			ppsFlag = 1;
			return false;
		}
		else{
			return true;
		}
		break;
	default:
		return false;
		break;
	}

	return 0;
}

int mp4Writer::get_data_size(unsigned char *pHead)
{
	char cSize[4] = { 0 };
	int *pDataSize = (int *)cSize;
	cSize[3] = pHead[0];
	cSize[2] = pHead[1];
	cSize[1] = pHead[2];
	cSize[0] = pHead[3];
	int size = *pDataSize;
	return size;
}

int mp4Writer::set_data_size(unsigned char *pHead, int size)
{
	char cSize[4] = { 0 };
	int *pDataSize = (int *)cSize;
	*pDataSize = size;
	pHead[0] = cSize[3];
	pHead[1] = cSize[2];
	pHead[2] = cSize[1];
	pHead[3] = cSize[0];

	return 0;
}

int mp4Writer::nal_parser_sub(unsigned char *pNal, nal_s &node)
{
	std::list<nal_s> lSons;
	std::vector<nal_s> &vSons = node.vecSons;
	int size = node.size + 4;
	// 找到所有的0x00000001
	for (int i = 0; i < size; i++){
		if (pNal[i] == 0x00 && pNal[i + 1] == 0x00 && pNal[i + 2] == 0x00 && pNal[i + 3] == 0x01){
			nal_s ns = { 0 };
			ns.posHead = i;
			ns.type = pNal[i + 4] & 0x1f;
			ns.bHasStartCode = true;
			lSons.push_back(ns);
		}
	}

	// 没有找到 0x00000001的情况，认为只包含一个NalU
	if (lSons.size() == 0){
		nal_s ns;
		ns.posHead = node.posHead;
		ns.type = pNal[4] & 0x1f;
		ns.bHasStartCode = false;
		ns.size = size - 4;
		vSons.push_back(ns);
		return 0;
	}

	// 处理第一个NalU
	if (lSons.front().posHead == 4){ // 第一个NalU包含0x00000001,调整位置
		lSons.front().posHead = 0;
	}else{ // 包含有0x00000001，但首个NalU没有0x00000001，也就是多slice的情况
		nal_s ns;
		ns.posHead = 0;
		ns.type = pNal[4] & 0x1f;
		ns.bHasStartCode = false;
		lSons.push_front(ns);
	}

	// 复制到node中
	std::list<nal_s>::iterator iter = lSons.begin();
	for (; iter != lSons.end(); iter++){
		vSons.push_back(*iter);
	}
#if 0
	// 只有一个nalu
	if (vSons.size() == 1){
		vSons[0].size = size - 4;
		vSons[0].posHead += node.posHead;
		return 0;
	}
#endif
	// 生成size信息
	int i;
	for (i = 0; i < vSons.size() - 1; i++){
		vSons[i].size = vSons[i + 1].posHead - vSons[i].posHead - 4;
	}
	vSons[i].size = size - vSons[i].posHead - 4;

	// 生成绝对位置
	for (i = 0; i < vSons.size(); i++){
		vSons[i].posHead += node.posHead;
	}

	// 统一去除startcode，除了第一个
	for (i = 1; i < vSons.size(); i++){
		vSons[i].bHasStartCode = false;
	}

	return 0;
}

int mp4Writer::nal_parser(AVPacket *org)
{
	//return 0;
	if (org->data == NULL || org->size <= 0)
		return 0;
	static int iii = 0;

	std::vector<nal_s> vecNalInfo;
	unsigned char *pNal = org->data;
	for (int i = 0; i < org->size;){
		nal_s ns = { 0 };
		ns.posHead = i;
		ns.type = -1;
		ns.size = get_data_size(&pNal[ns.posHead]);

		if (ns.size > org->size)
			return 0;
		nal_parser_sub(&pNal[ns.posHead], ns);
		vecNalInfo.push_back(ns);

		i += 4;
		i += ns.size;
	}
#if 0
	{
		printf("org->size:%d\n", org->size);
		for (int i = 0; i < vecNalInfo.size(); i++){
			printf("Nal%d\n", i);
			// 拷贝
			std::vector<nal_s> &vSons = vecNalInfo[i].vecSons;
			for (int sIdx = 0; sIdx < vSons.size(); sIdx++){
				nal_s &ns = vSons[sIdx];
				printf("%d %d %d\n", ns.type, ns.posData, ns.size);
			}
		}

	}
#endif
	// 遍历处理
	memcpy(_buffer, org->data, org->size);
	int curPos = 0;
	int totalSize = 0;
	for (int i = 0; i < vecNalInfo.size(); i++){

		// 拷贝
		std::vector<nal_s> &vSons = vecNalInfo[i].vecSons;
		int naluTotalSize = vecNalInfo[i].size;
		int curPktuFirstPos = curPos;
		bool isWriteFirst = false;
		int delCount = 0;
		for (int sIdx = 0; sIdx < vSons.size(); sIdx++){
			nal_s &ns = vSons[sIdx];
			if (is_delete(ns.type) == true){

			}else{
				if (ns.bHasStartCode == false){
					memcpy(&org->data[curPos + 4], &_buffer[ns.posHead+4], ns.size);
					set_data_size(&org->data[curPos], ns.size);
					curPos += ns.size;
					curPos += 4;
					totalSize = totalSize + ns.size + 4;
				}
				else{ //bHasStartCode==true
					memcpy(&org->data[curPos + 4], &_buffer[ns.posHead + 8], ns.size - 4);
					set_data_size(&org->data[curPos], ns.size-4);
					curPos += ns.size;
					totalSize = totalSize + ns.size;
				}
			}
		}
	}
	org->size = totalSize;
	//printf("cur size:%d\n\n", org->size);

	return 0;
}
