CC      := gcc
CFLAGS  := -Wall -Wextra -g -O2
TARGET  := main


SRC  := main.c
DEPS := \
	core/hash_table/hash-table.c core/hash_table/hash-table.h \
	parser/ast.c   parser/ast.h   \
	parser/lexer.c parser/lexer.h \
	parser/parser.c parser/parser.h \
	parser/eval.c  parser/eval.h  \
	globals/enums.h \
	utils/log.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC) $(DEPS)
	$(CC) $(CFLAGS) $(SRC) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
