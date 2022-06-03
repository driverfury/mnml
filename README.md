# MNML

This is an emulator made from scratch in C inspired by [Slu4](https://github.com/slu4coder)'s YouTube [series](https://youtube.com/playlist?list=PLYlQj5cfIcBU5SqFe6Uz4Q31_6VZyZ8h5).

The goal is to write an emulator and a toolchain (based on [Simplest CPU](https://github.com/slu4coder/Minimal-UART-CPU-System)) from scratch.

The toolchain will be built: assembler, compiler of our own language, OS and so on.

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

This is a very minimal B-like programming language. I write the compiler in C and use it as a bootstrap to build a self-hosted compiler in MIN.

The output of the compiler is, of course, assembly language for the simplest CPU.

The MIN language has a syntax similar to B and C.

How you can declare variables:
```
var variable1: char;    // char (= 1 byte)
var variable2: int;     // int  (= 2 bytes)
var variable3: [char];  // ptr  (= 2 bytes)
var variable3: int[3];  // array  (= length*base_type_width = 3*2 = 6)

// This is how you define a structure
struct MyStruct {
    first_member: int;
    second_member: int;
    third_member: char;
}

var variable4: MyStruct;  // struct (= sizeof(int) + sizeof(int) + sizeof(char) = 5)
```

A simple implementation of a FizzBuzz in MIN:
```
// The function main is the entry point
func main(): int
{
    var i: int;

    i = 1;
    while(i < 100)
    {
        if(i%15 == 0)
        {
            puts("FizzBuzz ");
        }
        else if(i%5 == 0)
        {
            puts("Buzz ");
        }
        else if(i%3 == 0)
        {
            puts("Fizz ");
        }
        else
        {
            puts("- ");
        }

        i = i + 1;
    }

    return 0;
}
```

## Assembler

I made a simple assembler in Python. I will use it as a bootstrap to build an assembler written in MIN.
