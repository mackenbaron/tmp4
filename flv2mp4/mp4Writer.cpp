
#include "mp4Writer.h"
#include <string>
#include <vector>
#include <list>

FILE *fp_open_v;
FILE *fp_open_a;

mp4Writer::mp4Writer()
{
	ifmt_v = NULL;
	ifmt_a = NULL;
	ofmt = NULL;
	ifmt_ctx_v = NULL;
	ifmt_ctx_a = NULL;
	ofmt_ctx = NULL;

	videoindex_v = -1;
	videoindex_out = -1;
	audioindex_a = -1;
	audioindex_out = -1;
	bHasAudio = 0;
	_buffer = new unsigned char[1024 * 1024];
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
	fp_open_v = fopen(in_filename_v.c_str(), "rb");
	if (fp_open_v == NULL)
	{

	}
	ifmt_ctx_v = avformat_alloc_context();
	unsigned char *iobuffer_v = (unsigned char *)av_malloc(IO_BUFFER_SIZE);
	AVIOContext *avio_v = avio_alloc_context(iobuffer_v, IO_BUFFER_SIZE, 0, NULL, fill_iobuffer_v, NULL, NULL);
	ifmt_ctx_v->pb = avio_v;

	fp_open_a = fopen(in_filename_a.c_str(), "rb");
	if (fp_open_a == NULL)
	{

	}
	ifmt_ctx_a = avformat_alloc_context();
	unsigned char *iobuffer_a = (unsigned char *)av_malloc(IO_BUFFER_SIZE);
	AVIOContext *avio_a = avio_alloc_context(iobuffer_a, IO_BUFFER_SIZE, 0, NULL, fill_iobuffer_a, NULL, NULL);
	ifmt_ctx_a->pb = avio_a;


	ifmt_v = av_find_input_format("flv");
	if ((ret = avformat_open_input(&ifmt_ctx_v, "", ifmt_v, NULL)) < 0) {
		printf("Could not open input file.");

		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		printf("Failed to retrieve input stream information");

		goto end;
	}
	printf("===========Input Information==========\n");
	av_dump_format(ifmt_ctx_v, 0, in_filename_v.c_str(), 0);
	printf("======================================\n");



	ifmt_a = av_find_input_format("flv");
	if ((ret = avformat_open_input(&ifmt_ctx_a, "nothing", ifmt_a, NULL)) < 0) {
		printf("Could not open input file.");

		bHasAudio = -1;
		//goto end;
	}
	if (bHasAudio == 0)
	{
		if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
			printf("Failed to retrieve input stream information");

			bHasAudio = -1;
			//goto end;
		}
		printf("===========Input Information==========\n");
		av_dump_format(ifmt_ctx_a, 0, in_filename_a.c_str(), 0);
		printf("======================================\n");
	}


	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename.c_str());
	if (!ofmt_ctx) {
		printf("Could not create output context\n");

		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;


	for (int i = 0; i < ifmt_ctx_v->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			AVStream *in_stream = ifmt_ctx_v->streams[i];
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
			videoindex_v = i;
			if (!out_stream) {
				printf("Failed allocating output stream\n");

				ret = AVERROR_UNKNOWN;
				goto end;
			}
			videoindex_out = out_stream->index;
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf("Failed to copy context from input to output stream codec context\n");

				goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}
	}

	if (bHasAudio == 0)
	{
		for (int i = 0; i < ifmt_ctx_a->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			if (ifmt_ctx_a->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
				AVStream *in_stream = ifmt_ctx_a->streams[i];
				AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
				audioindex_a = i;
				if (!out_stream) {
					printf("Failed allocating output stream\n");
					ret = AVERROR_UNKNOWN;
					goto end;
				}
				audioindex_out = out_stream->index;
				//Copy the settings of AVCodecContext
				if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
					printf("Failed to copy context from input to output stream codec context\n");
					goto end;
				}
				out_stream->codec->codec_tag = 0;
				if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
					out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

				break;
			}
		}

	}

	printf("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename.c_str(), 1);
	printf("======================================\n");
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename.c_str(), AVIO_FLAG_WRITE) < 0) {
			printf("Could not open output file '%s'", out_filename.c_str());

			goto end;
		}
	}
	
	return 0;
end:
	cleanup();
	return -1;
}

int mp4Writer::cleanup()
{
	int ret;
	avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	return 0;
}

int mp4Writer::writeMp4(Config &cfg)
{
	in_filename_v = cfg._inputFile;
	in_filename_a = cfg._inputFile;
	out_filename = cfg._outputFile;

	if (init() != 0)
		return -1;

	int64_t cur_pts_v=0;
	int64_t cur_pts_a=0;
	int frame_index=0;
	AVPacket pkt;
	
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf("Error occurred when opening output file\n");
		goto end;
	}

	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index = 0;
		AVStream *in_stream, *out_stream;

		//Get an AVPacket
		if (bHasAudio != 0)
		{
			ifmt_ctx = ifmt_ctx_v;
			stream_index = videoindex_out;

			if (av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if (pkt.stream_index == videoindex_v){
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1 = in_stream->time_base;
							//Duration between 2 frames (μs)
							int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
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
		else
		{
			if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
				ifmt_ctx = ifmt_ctx_v;
				stream_index = videoindex_out;

				if (av_read_frame(ifmt_ctx, &pkt) >= 0){
					do{
						in_stream = ifmt_ctx->streams[pkt.stream_index];
						out_stream = ofmt_ctx->streams[stream_index];

						if (pkt.stream_index == videoindex_v){
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
				ifmt_ctx = ifmt_ctx_a;
				stream_index = audioindex_out;
				if (av_read_frame(ifmt_ctx, &pkt) >= 0){
					do{
						in_stream = ifmt_ctx->streams[pkt.stream_index];
						out_stream = ofmt_ctx->streams[stream_index];
						if (pkt.stream_index == audioindex_a){
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
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf("Error muxing packet\n");
			break;
		}

		av_free_packet(&pkt);
	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

end:
	cleanup();
	return 0;
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
		}else{
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

	// 找到所有的0x00000001
	for (int i = 0; i < node.size; i++){
		if (pNal[i] == 0x00 && pNal[i + 1] == 0x00 && pNal[i + 2] == 0x00 && pNal[i + 3] == 0x01){
			nal_s ns = { 0 };
			ns.pos = i;
			ns.type = pNal[i + 4] & 0x1f;
			ns.bHasStartCode = true;
			lSons.push_back(ns);
		}
	}

	// 没有找到 0x00000001的情况，认为只包含一个NalU
	if (lSons.size() == 0){
		nal_s ns;
		ns.pos = node.pos+4;
		ns.type = pNal[4] & 0x1f;
		ns.bHasStartCode = false;
		ns.size = node.size-4;
		vSons.push_back(ns);
		return 0;
	}

	// 包含有0x00000001，但首个NalU没有0x00000001，也就是多slice的情况
	if (lSons.front().pos > 4){
		nal_s ns;
		ns.pos = 4;
		ns.type = pNal[4] & 0x1f;
		ns.bHasStartCode = false;
		lSons.push_front(ns);
	}

	// 复制到node中
	std::list<nal_s>::iterator iter = lSons.begin();
	for (; iter != lSons.end(); iter++){
		vSons.push_back(*iter);
	}

	// 只有一个nalu
	if (vSons.size() == 1){
		vSons[0].size = node.size-4;
		vSons[0].pos += node.pos;
		return 0;
	}

	// 生成size信息
	int i;
	for (i = 0; i < vSons.size()-1; i++){
		vSons[i].size = vSons[i + 1].pos - vSons[i].pos;
	}
	vSons[i].size = node.size - vSons[i].pos;

	// 生成绝对位置
	for (i = 0; i < vSons.size(); i++){
		vSons[i].pos += node.pos;
	}

	return 0;
}

int mp4Writer::nal_parser(AVPacket *org)
{

	if (org->data == NULL || org->size <= 0)
		return 0;

	std::vector<nal_s> vecNalInfo;
	unsigned char *pNal = org->data;
	for (int i = 0; i < org->size;){
			nal_s ns = {0};
			ns.pos = i;
			ns.type = -1;
			ns.size = get_data_size(&pNal[ns.pos]);
			ns.size += 4;
			
			nal_parser_sub(&pNal[i], ns);
			vecNalInfo.push_back(ns);
			
			i += ns.size;
	}
	
	// 遍历处理
	memcpy(_buffer, org->data, org->size);
	int curPos = 0;
	int totalSize = 0;
	for (int i = 0; i < vecNalInfo.size(); i++){

		// 拷贝
		std::vector<nal_s> &vSons = vecNalInfo[i].vecSons;
		int naluTotalSize = vecNalInfo[i].size-4;
		int curPktuFirstPos;
		bool isWriteFirst = false;
		for (int sIdx = 0; sIdx < vSons.size(); sIdx++){
			nal_s &ns = vSons[sIdx];
			if (is_delete(ns.type)==true){
				naluTotalSize -= ns.size;
			}else{
				if (isWriteFirst == true){
					memcpy(&org->data[curPos], &_buffer[ns.pos], ns.size);
					curPos += ns.size;
				}else{ //bIsFirst==true
					isWriteFirst = true;
					if (ns.bHasStartCode == false){
						curPktuFirstPos = curPos;
						memcpy(&org->data[curPos+4], &_buffer[ns.pos], ns.size);
						curPos += ns.size;
						curPos += 4;
					} else{ //bIsFirst==true && bHasStartCode==true
						curPktuFirstPos = curPos;
						memcpy(&org->data[curPos+4], &_buffer[ns.pos+4], ns.size-4);
						naluTotalSize -= 4;
					}
				}
			}
		}
		// 重新生成头大小
		set_data_size(&org->data[curPktuFirstPos], naluTotalSize);
		naluTotalSize += 4;
		totalSize += naluTotalSize;
	}
	org->size = totalSize;

	return 0;
}
