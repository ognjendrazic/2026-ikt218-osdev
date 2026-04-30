#include <libc/stdint.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>
#include <terminal.h>
#include <memory.h>
#include <pit.h>
#include "music_player/song.h"
#include "snake/snake.h"

extern uint32_t end; // Defined in arch/i386/linker.ld, marks where kernel image ends.

static void play_music(void) {
    Song songs[] = {
        {music_1, sizeof(music_1) / sizeof(Note)},
        {starwars_theme, sizeof(starwars_theme) / sizeof(Note)},
        {battlefield_1942_theme, sizeof(battlefield_1942_theme) / sizeof(Note)},
    };
    uint32_t n_songs = sizeof(songs) / sizeof(Song);

    SongPlayer *player = create_song_player();

    for (uint32_t i = 0; i < n_songs; i++) {
        printf("Playing song %d of %d...\n", i + 1, n_songs);
        player->play_song(&songs[i]);
        printf("Done.\n");
    }

    free(player);
    printf("Player stopped.  SPACE: play again   G: snake\n");
}

void main(uint32_t magic, void *mbi) {
    (void)magic;
    (void)mbi;

    gdt_init();
    terminal_init();
    printf("Launching Fanta OS\n\n");
    idt_init();

    // Test three ISRs (Task 2)
    printf("Testing ISRs...\n");
    asm volatile("int $0x0"); // #DE - Division Error
    asm volatile("int $0x3"); // #BP - Breakpoint
    asm volatile("int $0x6"); // #UD - Invalid Opcode
    printf("ISR test complete.\n");

    // Initialize memory manager and enable paging.
    init_kernel_memory(&end);
    init_paging();
    print_memory_layout();

    void *mem1 = malloc(12345);
    void *mem2 = malloc(54321);
    void *mem3 = malloc(13331);
    printf("malloc test: %x %x %x\n", (uint32_t)mem1, (uint32_t)mem2, (uint32_t)mem3);

    // Remap PIC, register handlers, initialize PIT, then enable interrupts.
    pic_init();
    keyboard_init();
    init_pit();
    asm volatile("sti");

    play_music();

    for (;;) {
        asm volatile("sti; hlt");
        char c = keyboard_getchar();
        if (c == ' ') play_music();
        if (c == 'g') {
            snake_run();
            // snake_run calls terminal_init() before returning, leaving the
            // screen blank, reprint a short prompt so the user knows what to do.
            printf("Fanta OS\n\nSPACE: Play music   G: Snake\n");
        }
    }
}
