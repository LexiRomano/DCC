int main();

void _start()
{

    __dsb__(
        "    .requires     __STACK_SPACE_START__\n"
        "    GETABS        OA _start\n"
        "    GETABS        OB __STACK_SPACE_START__\n"
    );

    main();

    __dsb__(
        "    TERM\n"
    );
}
