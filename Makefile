TARGET = ckw.exe
OBJ = main.obj selection.obj misc.obj option.obj ime_wrap.obj rsrc.res

#CFLAGS = -nologo -MLd -Od -W3 -GX -D_DEBUG
CFLAGS = -nologo -MT -O2 -W3 -EHsc -DNDEBUG
LDFLAGS = -nologo

UINC = 
ULIB = user32.lib gdi32.lib shell32.lib

.SUFFIXES: .exe .obj .cpp .rc .res

all: ver $(TARGET)

ver:
	version.bat > version.h

$(TARGET): $(OBJ)
	link $(LDFLAGS) -out:$@ $(OBJ) $(ULIB)

.cpp.obj:
	cl $(CFLAGS) $(UINC) -Fo$@ -c $<
.rc.res:
	rc -l 0x409 -fo $@ $<
clean:
	del $(OBJ) $(TARGET)
