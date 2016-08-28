bin_PROGRAMS = xpmem_chnl_serv
xpmem_chnl_serv_SOURCES = xpmem_chnl_serv.c
noinst_LIBRARIES = libxpmem_chnl_client.a
libxpmem_chnl_client_a_SOURCES = xpmem_chnl_client.c pmi-forward.c
GNI_HEADER=/home/smukher/gni-headers/cray-gni-headers-4.0/include
WHITEDB=../../src/pisces/hobbes/whitedb-0.7.3
HOBBS=../../src/pisces/hobbes/libhobbes
XPMEM=../../src/pisces/xpmem
UGNI=/opt/cray/ugni/default/lib64
ALPS=/opt/cray/alps/default
PMI=/opt/cray/pmi/default
#PMI_CFLAGS=$(shell pkg-config --cflags cray-pmi)
#PMI_LIBS=$(shell pkg-config --libs cray-pmi)
#UGNI_CFLAGS=$(shell pkg-config --cflags cray-ugni)
#UGNI_LIBS=$(shell pkg-config --libs cray-ugni)

BUILT_SOURCES=xmem_list.h
INCDIR= -I$(GNI_HEADER) -I$(ALPS)/include -I$(HOBBS) -I$(WHITEDB)/Db  -I$(XPMEM)/include -I$(PMI) -I/include -I/opt/cray/gni-headers/3.0-1.0502.9684.5.2.gem/include/  -I/opt/cray/pmi/5.0.7-1.0000.10678.155.29.gem/include -I../../src/pisces/petlib -I../../src/ompi/ompi/include
libxpmem_chnl_client_a_CFLAGS= -g  -Wall -fPIC -DCRAY_CONFIG_GHAL_GEMINI -Wunused-variable -Wunused-variable -c -fPIC -static -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR) 
#libxpmem_chnl_client_a_LDFLAGS=  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64 -L$(HOBBS) -L$(WHITEDB)/Db/.libs -L/opt/cray/pmi/5.0.7-1.0000.10678.155.29.gem/lib64/ 
#libxpmem_chnl_client_a_LDADD= -lm -lhobbes -lalpslli -lDb -lugni -lalpslli -lDb -lm -lhobbes

AM_LDFLAGS= -g

xpmem_chnl_serv_CFLAGS= -g  -Wall -fPIC -DCRAY_CONFIG_GHAL_GEMINI -Wunused-variable -Wunused-variable -fPIC -static -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR)
xpmem_chnl_serv_LDFLAGS=  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64 -L$(HOBBS) -L$(WHITEDB)/Db/.libs -L/opt/cray/pmi/5.0.7-1.0000.10678.155.29.gem/lib64/ -L/opt/cray/wlm_detect/1.0-1.0502.53341.1.1.gem/lib64/ -L../../src/ompi/ompi/.libs -L../../src/ompi/opal/.libs -L/opt/cray/udreg/2.3.2-1.0502.9275.1.25.gem/lib64/ -L../../src/numactl/.libs/
# order matters a lot here
xpmem_chnl_serv_LDADD= -lDb -lpmi -lhobbes -lalpslli -lalpsutil -lwlm_detect -lugni   -lrt -lpthread -lm
