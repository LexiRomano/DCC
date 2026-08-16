int main();

void _start()
{

    __dsb__(
        "    .requires     __STACK_SPACE_START__\n"
        "    GETABS        OA _start\n"
        "    GETABS        OB __STACK_SPACE_START__\n"
        "    MOVE          SB OB\n"
        "    MOVE          SP 0\n"
        "    MOVE          SS 0xFFFF\n"
    );

    main();

    // Until we get an actual operating system to handle finishing the program :P
    __dsb__(
        "    TERM\n"
    );
}
