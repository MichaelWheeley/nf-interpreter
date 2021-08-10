//
// Copyright (c) .NET Foundation and Contributors
// Portions Copyright (c) Microsoft Corporation.  All rights reserved.
// See LICENSE file in the project root for full license information.
//

#include "sys_io_ser_native.h"

///////////////////////////////////////////////////////////////////////////
// memory allocated here for the buffer has to be released by the caller //
///////////////////////////////////////////////////////////////////////////
HRESULT Library_sys_io_ser_native_System_IO_Ports_SerialPort::SetupWriteLine(
    CLR_RT_StackFrame &stack,
    char **buffer,
    uint32_t *length,
    bool *isNewAllocation)
{
    NANOCLR_HEADER();

    const char *text;
    const char *newLine;
    uint32_t textLength;
    uint32_t newLineLength;
    bool addNewLine;

    // get a pointer to the managed object instance (no need to check as this has already been done by the caller)
    CLR_RT_HeapBlock *pThis = stack.This();

    // get pointer to string
    text = stack.Arg1().RecoverString();

    // check for NULL string
    if (text != NULL)
    {
        // get length
        textLength = hal_strlen_s(text);

        // set buffer length
        *length = textLength;

        // new line parameter
        addNewLine = (bool)stack.Arg2().NumericByRefConst().u1;

        // should a new line be added?
        if (addNewLine)
        {
            // get string for new line
            newLine = pThis[FIELD___newLine].RecoverString();

            newLineLength = hal_strlen_s(newLine);

            // allocate memory for buffer
            *buffer = (char *)platform_malloc(textLength + newLineLength);

            // sanity check for successful allocation
            if (*buffer)
            {
                // clear buffer
                memset(*buffer, 0, *length);

                // flag allocation
                *isNewAllocation = true;

                // update buffer length
                *length += newLineLength;

                // concatenate both strings
                strcat(*buffer, text);
                strcat(*buffer, newLine);
            }
            else
            {
                NANOCLR_SET_AND_LEAVE(CLR_E_OUT_OF_MEMORY);
            }
        }
        else
        {
            // buffer pointing to text
            *buffer = (char *)text;

            // flag NO allocation
            *isNewAllocation = false;
        }
    }

    NANOCLR_NOCLEANUP();
}

bool Library_sys_io_ser_native_System_IO_Ports_SerialPort::GetLineFromRxBuffer(
    CLR_RT_HeapBlock *serialDevice,
    HAL_RingBuffer<uint8_t> *ringBuffer,
    uint8_t *&line)
{
    const char *newLine;
    uint32_t newLineLength;
    int32_t compareIndex;
    uint8_t *buffer;
    uint8_t *comparison;
    uint32_t matchCount = 0;
    uint32_t index;

    // clear line
    line = NULL;

    // check for anything in the buffer
    if (ringBuffer->Length() > 0)
    {
        // get new line from field
        newLine = serialDevice[FIELD___newLine].RecoverString();
        newLineLength = hal_strlen_s(newLine);
        // need to subtract one because we are 0 indexed
        newLineLength--;

        // better optimize to speed up search
        ringBuffer->OptimizeSequence();

        // grab pointer to buffer start
        buffer = ringBuffer->Reader();

        // search for latest new line char in the buffer
        for (index = 0; index < ringBuffer->Length(); index++)
        {
            if (*buffer == newLine[newLineLength])
            {
                matchCount = 1;

                if (newLineLength == 1)
                {
                    // found and nothing else to compare
                    break;
                }
                else
                {
                    if (index >= newLineLength)
                    {
                        // get pointer to the index before the last one
                        comparison = buffer;
                        comparison--;

                        // subtract one position, we've already check the last one
                        compareIndex = newLineLength - 1;

                        do
                        {
                            if (*comparison == newLine[compareIndex])
                            {
                                // found another match
                                matchCount++;

                                // move comparer to position before
                                comparison--;
                            }
                        } while (--compareIndex <= 0);
                    }
                }
            }

            buffer--;
        }

        // sequence found?
        if (matchCount == newLineLength)
        {
            // allocate memory for the string
            // index has the position of the last char of the "new line" string
            // need to add an extra position for the terminator
            line = (uint8_t *)platform_malloc(index + 1);

            if (line != NULL)
            {
                // clear memory
                memset(line, 0, index + 1);

                // the returned string DOES NOT include the new line string
                memcpy(line, ringBuffer->Reader(), index - newLineLength);
            }
        }
    }

    return line;
}