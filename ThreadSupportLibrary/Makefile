CC := gcc
CFLAGS := -g -Wall -m32

TARGETS := libtsl.a app test_queues test_join_exit test_thread_creation

all: $(TARGETS)

# Sources and Objects
TSL_SRC := tsl.c
TEST_QUEUES_SRC := test_queues.c
TCB_SRC := tcb.c 
TCB_OBJS := $(TCB_SRC:.c=.o)
TEST_THREAD_CREATION_SRC := test_thread_creation.c
TEST_JOIN_EXIT := test_join_exit.c


TSL_OBJS := $(TSL_SRC:.c=.o)
TEST_QUEUES_OBJS := $(TEST_QUEUES_SRC:.c=.o)
TEST_THREAD_CREATION_OBJS := $(TEST_THREAD_CREATION_SRC:.c=.o)
TEST_JOIN_EXIT_OBJS := $(TEST_JOIN_EXIT:.c=.o)


# Library
libtsl.a: $(TSL_OBJS) $(TCB_OBJS)
	ar rcs $@ $^

# Test Queues
test_queues: $(TEST_QUEUES_OBJS) libtsl.a
	$(CC) $(CFLAGS) -o $@ $(TEST_QUEUES_OBJS) -L. -ltsl

# Application
app: app.o libtsl.a
	$(CC) $(CFLAGS) -o $@ app.o -L. -ltsl


test_thread_creation: $(TEST_THREAD_CREATION_OBJS) libtsl.a
	$(CC) $(CFLAGS) -o $@ $(TEST_THREAD_CREATION_OBJS) -L. -ltsl

test_join_exit: $(TEST_JOIN_EXIT_OBJS) libtsl.a
	$(CC) $(CFLAGS) -o $@ $(TEST_JOIN_EXIT_OBJS) -L. -ltsl
	

# General rule for object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Optional: a target to run the test automatically
test: test_queues
	./test_queues

thread: test_thread_creation
		./test_thread_creation

joinexit: test_join_exit
		./test_join_exit
# Clean
clean:
	rm -rf core *.o $(TARGETS) libtsl.a