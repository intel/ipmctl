/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file defines a class to log trace entry and exit for CPP code
 */

#ifndef _LOG_ENTER_EXIT_H_
#define	_LOG_ENTER_EXIT_H_

//#include <persistence/logging.h>

/*
 * Log the entrance and exiting of a method/function automatically.
 */
class LogEnterExit
{
	public:
		/*
		 *
		 * @param funcName
		 *  Function name from which log is being written
		 * @param srcFile
		 *  File name from which log is being written
		 * @param lineNum
		 *  Line number within file from which log is being written
		 * @details
		 *  WARNING: Since no copying of data occurs, and the pointer to that data is maintained
		 *  for the life of this object, thus the constant string must exist for the
		 *  life of this obj or risk seg faulting. An alternative solution, but slower,
		 *  is to copy the strings internal to this obj during construction.
		 *
		 */
		LogEnterExit(const char *funcName, const char *srcFile, const int lineNum) :
			m_FuncName(funcName), m_SrcFile(srcFile), m_LineNum(lineNum)
		{
			/*
			log_trace_f(LOGGING_LEVEL_INFO, FLAG_PRINT_TRACE, m_SrcFile, m_LineNum, "Entering: %s",
					m_FuncName);*/
		}

		virtual ~LogEnterExit()
		{
			/*
			log_trace_f(LOGGING_LEVEL_INFO, FLAG_PRINT_TRACE, m_SrcFile, m_LineNum, "Exiting: %s",
					m_FuncName);*/
		}

	private:
		const char *m_FuncName;
		const char *m_SrcFile;
		const int m_LineNum;
};


#endif /* _LOG_ENTER_EXIT_H_ */
