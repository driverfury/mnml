# MNML

This is an emulator made from scratch in C inspired by [Slu4](https://github.com/slu4coder)'s YouTube [series](https://youtube.com/playlist?list=PLYlQj5cfIcBU5SqFe6Uz4Q31_6VZyZ8h5).

The goal is to write an emulator (based on [Simplest CPU](https://github.com/slu4coder/Minimal-UART-CPU-System)) from scratch in C.

A toolchain will be built: assembler, compiler of our own C-like language, OS and so on.

## Emulator

It's an emulator of the [Simplest CPU](https://github.com/slu4coder/Minimal-UART-CPU-System).

You can build the emulator with the following commad:

```bash
./gcc -ansi -Wall -Werror -DSTANDALONE -o emu emu.c
```

You can run a ROM with the following command:

```bash
./emu program.bin
```

If you want to run the emulator in debug mode:

```bash
./emu -d program.bin
```

## MIN programming language

This is a very minimal C-like programming language. I write the compiler in C and use it as a bootstrap to build a self-hosted compiler in MIN.

The output of the compiler is, of course, assembly language for the simplest CPU.

## Assembler

I made a simple assembler in Python. I will use it as a bootstrap to build an assembler written in MIN.
