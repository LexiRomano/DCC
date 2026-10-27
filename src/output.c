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
                    PUT(DSB_SUBI); fprintf(outputFile, "G%hhu OB OA\n", baseReg);
                    PUT(DSB_ADDI); fprintf(outputFile, "G%hhu G%hhu %u\n",
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
                            if (isTypeSigned(&var->type))
                            {
                                PUT(DSB_LOADSCOA);
                            }
                            else
                            {
                                PUT(DSB_LOADCOA);
                            }
                            break;
                        }
                        case 2:
                        {
                            if (isTypeSigned(&var->type))
                            {
                                PUT(DSB_LOADSHOA);
                            }
                            else
                            {
                                PUT(DSB_LOADHOA);
                            }
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_LOADWOA);
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
                            if (isTypeSigned(&var->type))
                            {
                                PUT(DSB_LOADSCOB);
                            }
                            else
                            {
                                PUT(DSB_LOADCOB);
                            }
                            break;
                        }
                        case 2:
                        {
                            if (isTypeSigned(&var->type))
                            {
                                PUT(DSB_LOADSHOB);
                            }
                            else
                            {
                                PUT(DSB_LOADHOB);
                            }
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_LOADWOB);
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
            fprintf(outputFile, "    TODO string literal\n");
            break;
        }
        case et_unary_e:
        {
            switch (exp->contents.unary.operation)
            {
                case op_bitwiseNot_e:
                case op_logicalNot_e:
                {
                    if (false == outputExpressionInternal(exp->contents.binary.operand1,
                                                          baseReg,
                                                          false))
                    {
                        return false;
                    }

                    if (op_logicalNot_e == exp->contents.unary.operation)
                    {
                        PUT(DSB_COMPI); fprintf(outputFile, "G%hhu 1\n", baseReg);
                        PUT(DSB_MOVE);  fprintf(outputFile, "G%hhu 0\n", baseReg);
                        PUT(DSB_BSLC);  fprintf(outputFile, "G%hhu G%hhu 1\n", baseReg, baseReg);
                    }

                    PUT(DSB_NOT); fprintf(outputFile, "G%hhu G%hhu\n", baseReg, baseReg);
                    break;
                }
                default:
                {
                    fprintf(outputFile, "    TODO unary operation %d\n",
                            exp->contents.unary.operation);
                    break;
                }
            }
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
                            PUT(DSB_STORCOA);
                            break;
                        }
                        case 2:
                        {
                            PUT(DSB_STORHOA);
                            break;
                        }
                        case 4:
                        {
                            PUT(DSB_STORWOA);
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
                case op_addition_e:
                case op_subtraction_e:
                case op_bitshiftLeft_e:
                case op_bitshiftRight_e:
                case op_bitwiseAnd_e:
                case op_bitwiseOr_e:
                case op_bitwiseXor_e:
                {
                    if (false == outputExpressionInternal(exp->contents.binary.operand1,
                                                          baseReg,
                                                          false) ||
                        false == outputExpressionInternal(exp->contents.binary.operand2,
                                                          baseReg + 1,
                                                          false))
                    {
                        return false;
                    }

                    switch (exp->contents.binary.operation)
                    {
                        case op_addition_e:
                        {PUT(DSB_ADDI);break;}

                        case op_subtraction_e:
                        {PUT(DSB_SUBI);break;}

                        case op_bitshiftLeft_e:
                        {PUT(DSB_BSLT);break;}

                        case op_bitshiftRight_e:
                        {PUT(DSB_BSRT);break;}

                        case op_bitwiseAnd_e:
                        {PUT(DSB_AND);break;}

                        case op_bitwiseOr_e:
                        {PUT(DSB_OR);break;}

                        case op_bitwiseXor_e:
                        {PUT(DSB_XOR);break;}

                        default:
                        {INTERNAL_ERROR;return false;}
                    }
                    fprintf(outputFile, "G%hhu G%hhu G%hhu\n", baseReg, baseReg, baseReg + 1);
                    break;
                }
                case op_equals_e:
                case op_notEquals_e:
                case op_greaterThan_e:
                case op_greaterEqual_e:
                case op_lessEqual_e:
                case op_lessThan_e:
                {
                    bool isSignedComparison = false;

                    if (false == outputExpressionInternal(exp->contents.binary.operand1,
                                                          baseReg,
                                                          false) ||
                        false == outputExpressionInternal(exp->contents.binary.operand2,
                                                          baseReg + 1,
                                                          false))
                    {
                        return false;
                    }

                    if (isTypeSigned(&exp->contents.binary.operand1->resultingType) &&
                        isTypeSigned(&exp->contents.binary.operand2->resultingType))
                    {
                        isSignedComparison = true;
                    }

                    switch (exp->contents.binary.operation)
                    {
                        case op_equals_e:
                        {PUT(DSB_EVALIEQ);break;}

                        case op_notEquals_e:
                        {PUT(DSB_EVALINE);break;}
                        
                        case op_greaterThan_e:
                        {
                            if (isSignedComparison)
                            {
                                PUT(DSB_EVALIGT);
                            }
                            else
                            {
                                PUT(DSB_EVALIHI);
                            }
                            break;
                        }
                        case op_greaterEqual_e:
                        {
                            if (isSignedComparison)
                            {
                                PUT(DSB_EVALIGE);
                            }
                            else
                            {
                                PUT(DSB_EVALIHS);
                            }
                            break;
                        }
                        case op_lessEqual_e:
                        {
                            if (isSignedComparison)
                            {
                                PUT(DSB_EVALILE);
                            }
                            else
                            {
                                PUT(DSB_EVALILS);
                            }
                            break;
                        }
                        case op_lessThan_e:
                        {
                            if (isSignedComparison)
                            {
                                PUT(DSB_EVALILT);
                            }
                            else
                            {
                                PUT(DSB_EVALILO);
                            }
                            break;
                        }
                        default:
                        {INTERNAL_ERROR;return false;}
                    }

                    fprintf(outputFile, "G%hhu G%hhu G%hhu\n", baseReg, baseReg, baseReg + 1);

                    break;
                }
                case op_logicalAnd_e:
                case op_logicalOr_e:
                {
                    int statementId = 0;

                    if (false == outputExpressionInternal(exp->contents.binary.operand1,
                                                          baseReg,
                                                          false))
                    {
                        return false;
                    }

                    statementId = ftell(outputFile);

                    PUT(DSB_COMPI); fprintf(outputFile, "G%hhu 0\n", baseReg);
                    if (op_logicalAnd_e == exp->contents.binary.operation)
                    {
                        PUT(DSB_BREQPC); fprintf(outputFile, "__andFail_%u__\n", statementId);
                    }
                    else
                    {
                        PUT(DSB_BRNEPC); fprintf(outputFile, "__orSuccess_%u__\n", statementId);
                    }
                    

                    if (false == outputExpressionInternal(exp->contents.binary.operand2,
                                                          baseReg,
                                                          false))
                    {
                        return false;
                    }

                    PUT(DSB_COMPI); fprintf(outputFile, "G%hhu 0\n", baseReg);
                    if (op_logicalAnd_e == exp->contents.binary.operation)
                    {
                        PUT(DSB_BREQPC); fprintf(outputFile, "__andFail_%u__\n", statementId);
                        PUT(DSB_MOVE);   fprintf(outputFile, "G%hhu 1\n", baseReg);
                        PUT(DSB_BRALPC); fprintf(outputFile, "__andSucceded_%u__\n", statementId);
                                         fprintf(outputFile, "    :__andFail_%u__\n", statementId);
                        PUT(DSB_MOVE);   fprintf(outputFile, "G%hhu 0\n", baseReg);
                                         fprintf(outputFile, "    :__andSucceded_%u__\n", statementId);
                    }
                    else
                    {
                        PUT(DSB_BRNEPC); fprintf(outputFile, "__orSuccess_%u__\n", statementId);
                        PUT(DSB_MOVE);   fprintf(outputFile, "G%hhu 0\n", baseReg);
                        PUT(DSB_BRALPC); fprintf(outputFile, "__orFailed_%u__\n", statementId);
                                         fprintf(outputFile, "    :__orSuccess_%u__\n", statementId);
                        PUT(DSB_MOVE);   fprintf(outputFile, "G%hhu 1\n", baseReg);
                                         fprintf(outputFile, "    :__orFailed_%u__\n", statementId);
                    }

                    break;
                }
                default:
                {
                    fprintf(outputFile, "    TODO binary operation %d\n",
                            exp->contents.binary.operation);
                    break;
                }
            }
            break;
        }
        case et_trinary_e:
        {
            fprintf(outputFile, "    TODO trinary\n");
            break;
        }
        case et_functionCall_e:
        {
            int           paramIndex     = 0;
            int           totalParamSize = 0;
            expression_t *e              = NULL;
            type_t       *t              = NULL;

            for (e = exp->contents.functionCall.firstParam; NULL != e; e = e->next)
            {
                if (false == outputExpressionInternal(e, baseReg + paramIndex, false))
                {
                    return false;
                }
                paramIndex++;
            }

            // We can't directly save the value of OB since that would not make
            // the code relocatable. Instead we push the difference, calculated
            // in OC
            PUT(DSB_SUBI); fprintf(outputFile, "OC OB OA\n");
            PUT(DSB_PUSH); fprintf(outputFile, "OC\n");

            // Align OC to the base of the next function frame
            PUT(DSB_ADDI); fprintf(outputFile, "OC SB SP\n");
            PUT(DSB_ADDI); fprintf(outputFile, "OC OC 4\n");

            e = exp->contents.functionCall.firstParam;
            t = exp->contents.functionCall.function->parameterTypes.first;
            for (int pi = 0; pi < paramIndex; pi++)
            {
                if (NULL == e ||
                    NULL == t)
                {
                    INTERNAL_ERROR;
                    return false;
                }

                switch (t->size)
                {
                    case 1:
                    {
                        PUT(DSB_STORCOC);
                        break;
                    }
                    case 2:
                    {
                        PUT(DSB_STORHOC);
                        break;
                    }
                    case 4:
                    {
                        PUT(DSB_STORWOC);
                        break;
                    }
                    default:
                    {
                        INTERNAL_ERROR;
                        printf("%d\n", e->resultingType.size);
                        return false;
                    }
                }
                fprintf(outputFile, "G%hhu %d\n", baseReg + pi, totalParamSize);
                totalParamSize += t->size;
                e = e->next;
                t = t->next;
            }

            // Align OB
            PUT(DSB_MOVE); fprintf(outputFile, "OB OC\n");

            // Branch
            PUT(DSB_BPALPC); fprintf(outputFile, "%s\n", exp->contents.functionCall.function->identifier);

            // Re-calculate OC since we might have lost it and we can't store it
            // before the function call
            PUT(DSB_MOVE); fprintf(outputFile, "OC OB\n");

            // Use return value
            if (false == exp->contents.functionCall.function->returnType.isVoid ||
                0     != exp->contents.functionCall.function->returnType.pointerDepth)
            {
                switch (exp->contents.functionCall.function->returnType.size)
                {
                    case 1:
                    {
                        PUT(DSB_LOADCOC);
                        break;
                    }
                    case 2:
                    {
                        PUT(DSB_LOADHOC);
                        break;
                    }
                    case 4:
                    {
                        PUT(DSB_LOADWOC);
                        break;
                    }
                    default:
                    {
                        INTERNAL_ERROR;
                        printf("%d\n", exp->contents.functionCall.function->returnType.size);
                        return false;
                    }
                }

                fprintf(outputFile, "G%hhu 0\n", baseReg);
            }



            // Re-align OB (we pushed the diff between OB and OA, so add OA back)
            PUT(DSB_POP);  fprintf(outputFile, "OB\n");
            PUT(DSB_ADDI); fprintf(outputFile, "OB OB OA\n");
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
            PUT(DSB_COMPI); fprintf(outputFile, "G0 0\n");
            PUT(DSB_BREQPC);
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
            PUT(DSB_BRALPC); fprintf(outputFile, "__doneIf_%u__\n", endId);
        }

        ifData = ifData->next;
    }

    fprintf(outputFile, "    :__doneIf_%u__\n", endId);
    return true;
}

static bool outputAssembly(assembly_t *dsb)
{
    if (NULL == dsb)
    {
        INTERNAL_ERROR;
        return false;
    }

    for (strll_t *s = dsb->first; NULL != s; s = s->next)
    {
        fprintf(outputFile, "%s", s->str);
    }

    return true;
}

static bool outputReturn(return_t *ret)
{
    if (NULL == ret)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (ret->value)
    {
        if (false == outputExpression(ret->value))
        {
            return false;
        }

        PUT(DSB_STORWOB); fprintf(outputFile, "G0 0\n");
    }
    PUT(DSB_BRALPC); fprintf(outputFile, "ret\n");

    return true;
}

static bool outputWhile(while_t *wle)
{
    unsigned int expressionId = 0;

    if (NULL == wle ||
        NULL == wle->condition)
    {
        INTERNAL_ERROR;
        return false;
    }

    expressionId = ftell(outputFile);

    fprintf(outputFile, "    :__while_top_%u__\n", expressionId);

    if (false == outputExpression(wle->condition))
    {
        return false;
    }

    PUT(DSB_COMPI); fprintf(outputFile, "G0 0\n");
    PUT(DSB_BREQPC); fprintf(outputFile, "__while_end_%u__\n", expressionId);

    if (false == outputStackFrame(&wle->loop))
    {
        return false;
    }

    PUT(DSB_BRALPC); fprintf(outputFile, "__while_top_%u__\n", expressionId);

    fprintf(outputFile, "    :__while_end_%u__\n", expressionId);

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
        PUT(DSB_ADDI); fprintf(outputFile, "SP SP %u\n", sf->varSize);
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
                break;
            }
            case assembly_e:
            {
                if (false == outputAssembly(vc->data))
                {
                    return false;
                }
                break;
            }
            case return_e:
            {
                if (false == outputReturn(vc->data))
                {
                    return false;
                }
                break;
            }
            case while_e:
            {
                if (false == outputWhile(vc->data))
                {
                    return false;
                }
                break;
            }
            default:
            {
                fprintf(outputFile, "    TODO %d\n", vc->type);
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
        PUT(DSB_SUBI); fprintf(outputFile, "SP SP %u\n", sf->varSize);
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
        fprintf(outputFile, "    .export %s\n", f->identifier);

        // Declare required symbols
        if (0 != strcmp(f->identifier, "_start"))
        {
            if (false == addSectionToConfig(f->identifier))
            {
                return false;
            }
            fprintf(outputFile, "    .requires __STACK_OVERFLOW__\n");
        }
        for (strll_t *s = f->requiredSymbols.first; NULL != s; s = s->next)
        {
            fprintf(outputFile, "    .requires %s\n", s->str);
        }

        // Check for stack overflow
        if (0 != strcmp(f->identifier, "_start"))
        {
            PUT(DSB_ADDI);   fprintf(outputFile, "G0 SP %u\n", f->definition.maxStackSize);
            PUT(DSB_COMPI);  fprintf(outputFile, "G0 0xFFC0\n");
            PUT(DSB_BRHSPC); fprintf(outputFile, "__STACK_OVERFLOW__\n");
        }

        if (false == outputStackFrame(&f->definition))
        {
            return false;
        }

        // Return routine
        fprintf(outputFile, "    :ret\n");
        if (0 == strcmp(f->identifier, "_start"))
        {
            // Intentionally cause a stack underflow, we should
            // exit via syscall
            PUT(DSB_MOVE); fprintf(outputFile, "SP 0\n");
        }
        else
        {
            PUT(DSB_SUBI); fprintf(outputFile, "SP OB SB\n");
        }
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

    if (false == addSourceToConfig(dsbFileName) ||
        false == outputGlobalVariables() ||
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
