# Stack Frames

## Terminology

There are two different types of stack at play here, to avoid any confusion:

**Hardware Stack:** The stack managed by the CPU using instructions such as `PUSH` and `POP`. These are used in relation to subroutine calls such as multiplication and division, and keeping the return vector from a function call.

**Stack Frame:** The language specific stack which is created and managed by the C program. These are used in relation to function calls, if/else/do/for/while/switch statements, or any code blocks within a set of `{` braces `}`. The stack frame contains space for stack variables, the hardware stack, and the return vector.

## Register Usage

**SB**: Initialized to the begining of the available space after the text/data and remains constant.

**SS**: Initialized to `0xFFFF` and remains constant. Stack size checking is done manually as the first step of a function being invoked to ensure that there will be no stack overflow. See more information at the bottom of this page.

**SP**: Moved manually to account for new stack frames.

**OB**: Points to the absolute address of the beginning of the frame's variables (excludes the return vector). Used to index the frame variables during the execution of the function.

**OC**: Points to the base of the variadic function argument list.

## Stack frame Layout

**NOTE:** In all of these diagrams, the `SP` and `OB` markers are assumed to be pointing to the lowest address within the labeled section.

```
High address | <empty> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector
```

Adding a new stack frame within the same function:

```
High address | <empty> <- SP
     ^       | hardware stack
     ^       | new frame variables
     ^       | frame variables <- OB
Low address  | return vector

NOTE: that adding a new stack frame within a function
can only happen when the hardware stack is empty. Simply
move the SP to past the end of the new stack variables.
and use that space as such.
```

Calling a function:

```
Initial state:
High address | <empty> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 1 (optional): Populate arguments
High address | new function arguments
     ^       | <reserved 4 bytes> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 2: Align OB
High address | new function arguments  <- OB
     ^       | <reserved 4 bytes> <- SP
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 3: Branch into function
High address | new function arguments <- SP/OB
     ^       | new function return vector 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

===== New function's code domain =====

Step 4: Check for stack overflow

Step 5: Align hardware stack
High address | new hardware stack (empty) <-SP
     ^       | new function non-argument frame variables
     ^       | new function arguments <- OB
     ^       | new function return vector 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 6: Begin function execution
```

Returning from a function:

```
Initial state
High address | new hardware stack (empty) <- SP
     ^       | new function frame variables <- OB
     ^       | new function return vector 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 1 (optional): Populate return value
High address | new hardware stack (empty) <- SP
     ^       | new function frame variables
     ^       | new function return value <- OB
     ^       | new function return vector 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector
NOTE: The return value goes at the lowest address within the 
frame variable space. For a function without frame variables, this
would be the hardware stack's space.

Step 2: Prepare SP for return
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- SP/OB
     ^       | new function return vector 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 3: Call RETURN instruction
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- OB
     ^       | <empty> <- SP 
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

===== Original function's code domain =====

Step 4: Re-align OB
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value
     ^       | <empty> <- SP 
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 5 (optional): Utilize return value
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- LOAD into G*
     ^       | <empty> <- SP 
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 6: Discard previous function's data
High address | <empty> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector
NOTE: This is not an actual series of instructions. This is a
step for your mental model. The actual layout is the exact same as
Step 4/5

Step 7: Resume original function's execution
```

## Variadic functions

Calling a function:

```
Initial state:
High address | <empty> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 1 (optional): Populate variadic arguments
High address | new function variadic arguments <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 2: Populate known arguments
High address | new function arguments
     ^       | <reserved 4 bytes>
     ^       | new function variadic arguments <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 3: Align SP/OB/OC
High address | new function arguments  <- OB
     ^       | <reserved 4 bytes> <- SP
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 4: Branch into function
High address | new function arguments <- SP/OB
     ^       | new function return vector 
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

===== New function's code domain =====

Step 5: Check for stack overflow

Step 6: Align hardware stack
High address | new hardware stack (empty) <-SP
     ^       | new function non-argument frame variables
     ^       | new function arguments <- OB
     ^       | new function return vector
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 7: Begin function execution
```

Returning from a function:

```
Initial state
High address | new hardware stack (empty) <- SP
     ^       | new function frame variables <- OB
     ^       | new function return vector 
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 1 (optional): Populate return value
High address | new hardware stack (empty) <- SP
     ^       | new function frame variables
     ^       | new function return value <- OB
     ^       | new function return vector 
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 2: Prepare SP for return
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- SP/OB
     ^       | new function return vector
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

Step 3: Call RETURN instruction
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- OB
     ^       | <empty> <- SP
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables
Low address  | return vector

===== Original function's code domain =====

Step 4: Re-align OB
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value
     ^       | <empty> <- SP 
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 5 (optional): Utilize return value
High address | new hardware stack (empty)
     ^       | new function frame variables
     ^       | new function return value <- LOAD into G*
     ^       | <empty> <- SP
     ^       | new function variadic arguments <- OC
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 6: Discard previous function's data and align SP
High address | <empty> <- SP
     ^       | hardware stack
     ^       | frame variables <- OB
Low address  | return vector

Step 7: Resume original function's execution
```

## Stack Overflow

Stack overflows will be immediately checked by the called function. A stack overflow condition is met if the deepest expected stack frame within the function would result in the highest addressed byte in the stack being greater or equal to `0xFFC0`. This gives a `0x20` byte buffer to provide space for the hardware stack and (hopefully) avoid a hardware stack overflow. In the case of a stack overflow, the program will clear the stack and jump to an exit routine.
