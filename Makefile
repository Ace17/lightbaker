BIN?=bin

CXXFLAGS+=-O3

SRCS:=\
	src/main.cpp\
	src/packer.cpp\
	src/lightmap.cpp\


$(BIN)/lb.exe: $(SRCS:%=$(BIN)/%.o)

#------------------------------------------------------------------------------

clean:
	rm -rf $(BIN)

$(BIN)/%.exe:
	@mkdir -p $(dir $@)
	$(CXX) -o "$@" $^

$(BIN)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) -o "$@" $^
