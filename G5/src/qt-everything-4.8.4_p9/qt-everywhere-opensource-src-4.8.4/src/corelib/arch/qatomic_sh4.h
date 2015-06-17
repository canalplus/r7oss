/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QATOMIC_SH4_H
#define QATOMIC_SH4_H

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

#define Q_ATOMIC_INT_REFERENCE_COUNTING_IS_ALWAYS_NATIVE

inline bool QBasicAtomicInt::isReferenceCountingNative()
{ return true; }
inline bool QBasicAtomicInt::isReferenceCountingWaitFree()
{ return false; }

#define Q_ATOMIC_INT_TEST_AND_SET_IS_ALWAYS_NATIVE

inline bool QBasicAtomicInt::isTestAndSetNative()
{ return true; }
inline bool QBasicAtomicInt::isTestAndSetWaitFree()
{ return false; }

#define Q_ATOMIC_INT_FETCH_AND_STORE_IS_ALWAYS_NATIVE

inline bool QBasicAtomicInt::isFetchAndStoreNative()
{ return true; }
inline bool QBasicAtomicInt::isFetchAndStoreWaitFree()
{ return false; }

#define Q_ATOMIC_INT_FETCH_AND_ADD_IS_ALWAYS_NATIVE

inline bool QBasicAtomicInt::isFetchAndAddNative()
{ return true; }
inline bool QBasicAtomicInt::isFetchAndAddWaitFree()
{ return false; }

#define Q_ATOMIC_POINTER_TEST_AND_SET_IS_ALWAYS_NATIVE

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isTestAndSetNative()
{ return true; }
template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isTestAndSetWaitFree()
{ return false; }

#define Q_ATOMIC_POINTER_FETCH_AND_STORE_IS_ALWAYS_NATIVE

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isFetchAndStoreNative()
{ return true; }
template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isFetchAndStoreWaitFree()
{ return false; }

#define Q_ATOMIC_POINTER_FETCH_AND_ADD_IS_ALWAYS_NATIVE

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isFetchAndAddNative()
{ return true; }
template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::isFetchAndAddWaitFree()
{ return false; }

// Reference counting

inline bool QBasicAtomicInt::ref()
{
    register int newValue;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-6, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[newValue] 	\n" /* load old value */
	"  add #1, %[newValue] 			\n" /* Add 1 */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* store */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [newValue] "=&r" (newValue), "+m" (_q_value)
	: [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return newValue != 0;
}

inline bool QBasicAtomicInt::deref()
{
    register int newValue;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-6, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[newValue] 	\n" /* load old value */
	"  add #-1, %[newValue] 		\n" /* subtract 1 */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* store */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [newValue] "=&r" (newValue), "+m" (_q_value)
	: [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return newValue != 0;
}

// Test and set for integers

inline bool QBasicAtomicInt::testAndSetOrdered(int expectedValue, int newValue)
{
    register int old, ret;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-10, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  cmp/eq %[expectedValue], %[old]	\n" /* Compare old to expected */
	"  bf/s	1f				\n" /* Don't store if != */
	"    movt  %[ret]			\n" /* Return value */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* Store newValue */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), [ret] "=&r" (ret), "+m" (_q_value)
	: [newValue] "r" (newValue), [expectedValue] "r" (expectedValue),
	  [_q_value_ptr] "r" (&_q_value)
	: "cc", "r0", "r1"
    );


    return (bool) ret;
}

inline bool QBasicAtomicInt::testAndSetRelaxed(int expectedValue, int newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

inline bool QBasicAtomicInt::testAndSetAcquire(int expectedValue, int newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

inline bool QBasicAtomicInt::testAndSetRelease(int expectedValue, int newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

// Fetch and store for integers

inline int QBasicAtomicInt::fetchAndStoreOrdered(int newValue)
{
    register int old;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  nop					\n" /* For alignment */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-4, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* Store new value */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), "+m" (_q_value)
	: [newValue] "r" (newValue), [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return old;
}

inline int QBasicAtomicInt::fetchAndStoreRelaxed(int newValue)
{
    return fetchAndStoreOrdered(newValue);
}

inline int QBasicAtomicInt::fetchAndStoreAcquire(int newValue)
{
    return fetchAndStoreOrdered(newValue);
}

inline int QBasicAtomicInt::fetchAndStoreRelease(int newValue)
{
    return fetchAndStoreOrdered(newValue);
}

// Fetch and add for integers

inline int QBasicAtomicInt::fetchAndAddOrdered(int valueToAdd)
{
    register int old, tmp;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  nop					\n" /* Align */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-8, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  mov %[old], %[tmp]			\n" /* Cannot add to valueToAdd, cannot replay */
	"  add %[valueToAdd], %[tmp]		\n" /* Add on */
	"  mov.l %[tmp], @%[_q_value_ptr]	\n" /* Store new value */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), [tmp] "=&r" (tmp), "+m" (_q_value)
	: [valueToAdd] "r" (valueToAdd), [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return old;
}

inline int QBasicAtomicInt::fetchAndAddRelaxed(int valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

inline int QBasicAtomicInt::fetchAndAddAcquire(int valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

inline int QBasicAtomicInt::fetchAndAddRelease(int valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

// Test and set for pointers

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::testAndSetOrdered(T *expectedValue, T *newValue)
{
    register T *old;
    register int ret;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-10, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  cmp/eq %[expectedValue], %[old]	\n" /* Compare old to expected */
	"  bf/s	1f				\n" /* Don't store if != */
	"    movt %[ret]			\n" /* Return value */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* Store newValue */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), [ret] "=&r" (ret), "+m" (_q_value)
	: [expectedValue] "r" (expectedValue), [_q_value_ptr] "r" (&_q_value),
	  [newValue] "r" (newValue)
	: "cc", "r0", "r1"
    );

    return (bool) ret;
}

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::testAndSetRelaxed(T *expectedValue, T *newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::testAndSetAcquire(T *expectedValue, T *newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

template <typename T>
Q_INLINE_TEMPLATE bool QBasicAtomicPointer<T>::testAndSetRelease(T *expectedValue, T *newValue)
{
    return testAndSetOrdered(expectedValue, newValue);
}

// Fetch and store for pointers

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndStoreOrdered(T *newValue)
{
    register T *old;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  nop					\n" /* For mova align */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-4, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  mov.l %[newValue], @%[_q_value_ptr]	\n" /* Store new value */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), "+m" (_q_value)
	: [newValue] "r" (newValue), [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return old;
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndStoreRelaxed(T *newValue)
{
    return fetchAndStoreOrdered(newValue);
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndStoreAcquire(T *newValue)
{
    return fetchAndStoreOrdered(newValue);
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndStoreRelease(T *newValue)
{
    return fetchAndStoreOrdered(newValue);
}

// Fetch and add for pointers

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndAddOrdered(qptrdiff valueToAdd)
{
    register T *old, *tmp;

    asm volatile (
	"  .balign 4				\n"
	"  mova 1f, r0  			\n" /* r0 is end point */
	"  nop					\n" /* Align */
	"  mov r15, r1  			\n" /* r1 is saved SP */
	"  mov #-8, r15 			\n" /* LOGIN: r15 = size */
	"  mov.l @%[_q_value_ptr], %[old]	\n" /* Load old value */
	"  mov %[old], %[tmp]			\n" /* Cannot use valueToAdd, cannot replay */
	"  add %[valueToAdd], %[tmp]		\n" /* Add on */
	"  mov.l %[tmp], @%[_q_value_ptr]	\n" /* Store new value */
	"1:mov r1, r15				\n" /* LOGOUT */
	: [old] "=&r" (old), [tmp] "=&r" (tmp), "+m" (_q_value)
	: [valueToAdd] "r" (valueToAdd), [_q_value_ptr] "r" (&_q_value)
	: "r0", "r1"
    );

    return old;
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndAddRelaxed(qptrdiff valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndAddAcquire(qptrdiff valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

template <typename T>
Q_INLINE_TEMPLATE T *QBasicAtomicPointer<T>::fetchAndAddRelease(qptrdiff valueToAdd)
{
    return fetchAndAddOrdered(valueToAdd);
}

QT_END_NAMESPACE

QT_END_HEADER

#endif // QATOMIC_SH_H
