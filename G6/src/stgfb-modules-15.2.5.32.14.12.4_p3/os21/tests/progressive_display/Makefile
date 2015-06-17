
TARGET=progressive_display.out

all: $(TARGET)

$(TARGET): $(TARGET:.out=.o) $(LIB_SUBDIR)/libstgos21.a
	$(LD) $< $(LDFLAGS) -o $@

$(TARGET:.out=.o): $(TARGET:.out=.cpp)
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm -f $(TARGET)
