#include "../include/terminal.h"

int main(int argc, char** argv) {
    BUFFER.hold_buffer();
    VFS::vfs::get_vfs()->load_disks();

    VFS::terminal* term = VFS::terminal::get_instance();
    term->run();

    delete term;
    pthread_exit(0); // wait for all threads to die.
}