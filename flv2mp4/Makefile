# Makefile

all: static

SOURCE = flv2mp4.cpp mp4Writer.cpp mp4Writer.h
TARGET = flv2mp4
FFMPEG_PATH = /home/maozheng/local/ffmpeg
X264_PATH = /home/maozheng/local/x264

dynamic : $(SOURCE)
	g++ -o $(TARGET) $(SOURCE) \
		-D__STDC_CONSTANT_MACROS \
	-I$(FFMPEG_PATH)/include  -lavcodec   -lavfilter  -lavutil    -lswresample  -lavdevice  -lavformat  -lpostproc  -lswscale

static : $(SOURCE)
	g++ -o $(TARGET) $(SOURCE) \
		-D__STDC_CONSTANT_MACROS \
		$(FFMPEG_PATH)/lib/libavcodec.a  \
		$(FFMPEG_PATH)/lib/libavfilter.a \
		$(FFMPEG_PATH)/lib/libavdevice.a \
		$(FFMPEG_PATH)/lib/libavformat.a \
		$(FFMPEG_PATH)/lib/libavcodec.a  \
		$(FFMPEG_PATH)/lib/libpostproc.a \
		$(FFMPEG_PATH)/lib/libswscale.a  \
		$(FFMPEG_PATH)/lib/libavutil.a   \
		$(FFMPEG_PATH)/lib/libswresample.a   \
		$(X264_PATH)/lib/libx264.a       \
		-lpthread -lz -ldl -lbz2  -lrt 

clean:
	rm $(TARGET) -f


