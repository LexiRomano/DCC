int main();

void _start()
{

    __dsb__(
        "    .requires __STACK_SPACE_START__\n"
        "    getabs   OA _start\n"
        "    getabs   OB __STACK_SPACE_START__\n"
        "    move     SB OB\n"
        "    move     SP 0\n"
        "    move     SS 0xFFFF\n"
    );

    main();

    // Until we get an actual operating system to handle finishing the program :P
    __dsb__(
        "    bralpc   0\n"
    );
}
