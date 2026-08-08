#include "dcc.h"

static FILE         *outputFile = NULL;
static parsedData_t *pd         = NULL;

#define PUT(str) fprintf(outputFile, str);

static bool outputGlobalVariables()
{
    FILE *globalFile = NULL;

    globalFile = fopen(GLOBALS_DSB_FILE, "a");

    if (NULL == globalFile)
    {
        INTERNAL_ERROR;
        return false;
    }

    for (variable_t *v = pd->globalVariables.first; NULL != v; v = v->next)
    {
        fprintf(globalFile, "    .export  %s\n", v->identifier);
        fprintf(globalFile, "    .reserve %u\n", v->type.size);
    }

    fclose(globalFile);

    return true;
}

static bool outputExpressionInternal(expression_t *exp, uint8_t baseReg, bool forAssignment)
{
    switch (exp->expressionType)
    {
        case et_variable_e:
        {
            variable_t *var = exp->contents.variable;
            if (forAssignment)
            {
                // TODO optimize for direct assignment
                if (-1 == var->offsetInFunc)
                {
                    // Global variable
                    PUT(DSB_MOVE); fprintf(outputFile, "G%hhu %s\n", baseReg, var->identifier);
                }
                else
                {
                    // Somewhere in the function frame
                    PUT(DSB_SUB); fprintf(outputFile, "G%hhu OB OA\n", baseReg);
                    PUT(DSB_ADD); fprintf(outputFile, "G%hhu G%hhu %u\n",
                                          baseReg, baseReg, var->offsetInFunc);
                }
            }
            else
            {
                if (-1 == var->offsetInFunc)
                {
                    // global variable
                    switch (var->type.size)
                    {
                        case 1:
                        {
                            PUT(DSB_LOAD_C_OA);
                            break;
                        }
                        case 2:
                        {
                            PUT(DSB_LOAD_H_OA);
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_LOAD_OA);
                            break;
                        }
                        default:
                        {
                            INTERNAL_ERROR;
                            return false;
                        }
                    }

                    fprintf(outputFile, "G%hhu %s\n", baseReg, var->identifier);
                }
                else
                {
                    // Somewhere in the function frame
                                        switch (var->type.size)
                    {
                        case 1:
                        {
                            PUT(DSB_LOAD_C_OB);
                            break;
                        }
                        case 2:
                        {
                            PUT(DSB_LOAD_H_OB);
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_LOAD_OB);
                            break;
                        }
                        default:
                        {
                            INTERNAL_ERROR;
                            return false;
                        }
                    }

                    fprintf(outputFile, "G%hhu %u\n", baseReg, var->offsetInFunc);
                }
            }
            break;
        }
        case et_literal_e:
        {
            PUT(DSB_MOVE); fprintf(outputFile, "G%hhu %u\n", baseReg, exp->contents.literal);
            break;
        }
        case et_stringLiteral_e:
        {
            fprintf(outputFile, "    //TODO string literal\n");
            break;
        }
        case et_unary_e:
        {
            fprintf(outputFile, "    //TODO unary\n");
            break;
        }
        case et_binary_e:
        {
            switch (exp->contents.binary.operation)
            {
                case op_assignment_e:
                {
                    // TODO optimize for direct assignment
                    if (false == outputExpressionInternal(exp->contents.binary.operand1,
                                                          baseReg,
                                                          false) ||
                        false == outputExpressionInternal(exp->contents.binary.operand2,
                                                          baseReg + 1,
                                                          true))
                    {
                        return false;
                    }

                    switch (exp->contents.binary.operand2->resultingType.size)
                    {
                        case 1:
                        {
                            PUT(DSB_STOR_C_OA);
                            break;
                        }
                        case 2:
                        {
                            PUT(DSB_STOR_H_OA);
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_STOR_OA);
                            break;
                        }
                        default:
                        {
                            INTERNAL_ERROR;
                            return false;
                        }
                    }

                    fprintf(outputFile, "G%hhu G%hhu\n", baseReg, baseReg + 1);
                    break;
                }
                default:
                {
                    fprintf(outputFile, "    //TODO binary operation %d\n",
                            exp->contents.binary.operation);
                    break;
                }
            }
            break;
        }
        case et_trinary_e:
        {
            fprintf(outputFile, "    //TODO trinary\n");
            break;
        }
        case et_functionCall_e:
        {
            fprintf(outputFile, "    //TODO: function call\n");
            break;
        }
    }
    return true;
}

static bool outputExpression(expression_t *exp)
{
    if (NULL == exp)
    {
        INTERNAL_ERROR;
        return false;
    }

    return outputExpressionInternal(exp, 0, false);
}

// Forward declaration
static bool outputStackFrame(stackFrame_t *sf);

static bool outputIf(if_t *ifData)
{
    unsigned int endId   = 0;
    unsigned int skipId  = 0;
    bool         started = false;

    if (NULL == ifData)
    {
        INTERNAL_ERROR;
        return false;
    }

    endId = ftell(outputFile);

    while (ifData != NULL)
    {
        if (started)
        {
            fprintf(outputFile, "    :__skipIf_%u__\n", skipId);
        }
        started = true;

        if (NULL != ifData->condition)
        {
            if (NULL != ifData->next)
            {
                skipId = ftell(outputFile);
            }

            outputExpression(ifData->condition);
            PUT(DSB_COMP); fprintf(outputFile, "G0 0\n");
            PUT(DSB_BREQ);
            if (NULL == ifData->next)
            {
                fprintf(outputFile, "__doneIf_%u__\n", endId);
            }
            else
            {
                fprintf(outputFile, "__skipIf_%u__\n", skipId);
            }
        }

        outputStackFrame(&ifData->consequence);

        if (NULL != ifData->next)
        {
            PUT(DSB_BRAL); fprintf(outputFile, "__doneIf_%u__\n", endId);
        }

        ifData = ifData->next;
    }

    fprintf(outputFile, "    :__doneIf_%u__\n", endId);
    return true;
}

static bool outputStackFrame(stackFrame_t *sf)
{
    if (NULL == sf)
    {
        INTERNAL_ERROR;
        return false;
    }

    // Align SP
    if (sf->varSize > 0)
    {
        PUT(DSB_ADD); fprintf(outputFile, "SP SP %u\n", sf->varSize);
    }

    for (voidContainer_t *vc = sf->codeBlock.firstItem; vc != NULL; vc = vc->nextVoidContainer)
    {
        switch (vc->type)
        {
            case stackFrame_e:
            {
                if (false == outputStackFrame(vc->data))
                {
                    return false;
                }

                break;
            }
            case expression_e:
            {
                if (false == outputExpression(vc->data))
                {
                    return false;
                }
                break;
            }
            case if_e:
            {
                if (false == outputIf(vc->data))
                {
                    return false;
                }
            }
            default:
            {
                continue;
            }
        }
    }

    // Restore SP
    if (sf->varSize > 0 && NULL != sf->prevAccessableStackFrame)
    {
        // If the previous stack frame is NULL, we're in the first frame
        // of the function. In that case, the return routine will
        // deal with the SP. Also if this is a non-void function, this
        // should be inaccessable due to a return being required before
        // hitting the end of the root stack frame.
        PUT(DSB_SUB); fprintf(outputFile, "SP SP %u\n", sf->varSize);
    }

    return true;
}

static bool outputFunctions()
{
    for (function_t *f = pd->functions.first; NULL != f; f = f->next)
    {
        if (false == f->isDefined)
        {
            continue;
        }

        fprintf(outputFile, "\n.section %s\n", f->identifier);

        // Declare required symbols
        fprintf(outputFile, "    .requires __STACK_OVERFLOW__\n");
        for (strll_t *s = f->requiredSymbols.first; NULL != s; s = s->next)
        {
            fprintf(outputFile, "    .requires %s\n", s->str);
        }

        // Check for stack overflow
        PUT(DSB_ADD);  fprintf(outputFile, "G0 SP %u\n", f->definition.maxStackSize);
        PUT(DSB_COMP); fprintf(outputFile, "G0 0xFFC0\n");
        PUT(DSB_BRHS); fprintf(outputFile, "__STACK_OVERFLOW__\n");

        if (false == outputStackFrame(&f->definition))
        {
            return false;
        }

        // Return routine
        fprintf(outputFile, "    :ret\n");
        PUT(DSB_MOVE); fprintf(outputFile, "SP OB\n");
        PUT(DSB_RETURN);
    }
    return true;
}

bool output(parsedData_t *parsedData, char *dsbFileName)
{
    bool rc = true;

    if (NULL == parsedData)
    {
        return false;
    }

    pd = parsedData;

    if (NULL == dsbFileName)
    {
        INTERNAL_ERROR;
        freeParsedDataContents(parsedData);
        free(parsedData);
        return false;
    }

    outputFile = fopen(dsbFileName, "w");

    if (NULL == outputFile)
    {
        INTERNAL_ERROR;
        freeParsedDataContents(parsedData);
        free(parsedData);
        return false;
    }

    if (false == outputGlobalVariables() ||
        false == outputFunctions())
    {
        rc = false;
    }

    freeParsedDataContents(parsedData);
    free(parsedData);

    fclose(outputFile);

    if (false == rc)
    {
        remove(dsbFileName);
    }

    return rc;
}
