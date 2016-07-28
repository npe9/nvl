bin_PROGRAMS = xpmem_chnl_serv
xpmem_chnl_serv_SOURCES = xpmem_chnl_serv.c
noinst_LIBRARIES = libxpmem_chnl_client.a
libxpmem_chnl_client_a_SOURCES = xpmem_chnl_client.c
GNI_HEADER=/home/smukher/gni-headers/cray-gni-headers-4.0/include
WHITEDB=/home/smukher/hobbes/whitedb-0.7.3
HOBBS=/home/smukher/hobbes/libhobbes
XPMEM=/home/smukher/xpmem
UGNI=/opt/cray/ugni/default/lib64
ALPS=/opt/cray/alps/default
PMI=/opt/cray/pmi/default
#PMI_CFLAGS=$(shell pkg-config --cflags cray-pmi)
#PMI_LIBS=$(shell pkg-config --libs cray-pmi)
#UGNI_CFLAGS=$(shell pkg-config --cflags cray-ugni)
#UGNI_LIBS=$(shell pkg-config --libs cray-ugni)

BUILT_SOURCES=xmem_list.h
INCDIR= -I$(GNI_HEADER) -I$(ALPS)/include -I$(HOBBS) -I$(WHITEDB)/Db  -I$(XPMEM)/include -I$(PMI) -I/include/opt/cray/gni-headers/3.0-1.0502.9684.5.2.gem/include/  -I/opt/cray/pmi/5.0.7-1.0000.10678.155.29.gem/include

LIBDIR=  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64 -L$(HOBBS) -L$(WHITEDB)/Db/.libs -lugni -lalpslli -lDb -lm -lhobbes
libxpmem_chnl_client_a_CFLAGS= -g  -Wall -fPIC -DCRAY_CONFIG_GHAL_GEMINI -Wunused-variable -Wunused-variable -c -fPIC -static -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR) $(LIBDIR) -lm -lhobbes -lalpslli -lDb -lugni
AM_LDFLAGS= -g

xpmem_chnl_serv_CFLAGS= -g  -Wall -fPIC -DCRAY_CONFIG_GHAL_GEMINI -Wunused-variable -Wunused-variable -c -fPIC -static -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR) $(LIBDIR) -lm -lhobbes -lalpslli -lDb -lugni
