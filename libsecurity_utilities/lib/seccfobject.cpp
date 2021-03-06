/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "seccfobject.h"
#include "cfclass.h"
#include "errors.h"
#include "debugging.h"

#include <list>
#include "globalizer.h"
#include "auto_zone.h"

SecPointerBase::SecPointerBase(const SecPointerBase& p)
{
	if (p.ptr)
	{
		CFRetain(p.ptr->operator CFTypeRef());
	}
	ptr = p.ptr;
}




static void CheckForRelease(SecCFObject* ptr)
{
	CFTypeRef tr = ptr->operator CFTypeRef();
	CFIndex retainCount = CFGetRetainCount(tr);
	if (retainCount == 1 || retainCount == -1)
	{
		ptr->aboutToDestruct();
	}
}



SecPointerBase::SecPointerBase(SecCFObject *p)
{
	if (p && !p->isNew())
	{
		CFRetain(p->operator CFTypeRef());
	}
	ptr = p;
}



SecPointerBase::~SecPointerBase()
{
	if (ptr)
	{
		CheckForRelease(ptr);
		CFRelease(ptr->operator CFTypeRef());
	}
}



SecPointerBase& SecPointerBase::operator = (const SecPointerBase& p)
{
	if (p.ptr)
	{
		CFTypeRef tr = p.ptr->operator CFTypeRef();
		CFRetain(tr);
	}
	if (ptr)
	{
		CheckForRelease(ptr);
		CFRelease(ptr->operator CFTypeRef());
	}
	ptr = p.ptr;
	return *this;
}



void SecPointerBase::assign(SecCFObject * p)
{
	if (p && !p->isNew())
	{
		CFRetain(p->operator CFTypeRef());
	}
	if (ptr)
	{
		CheckForRelease(ptr);
		CFRelease(ptr->operator CFTypeRef());
	}
	ptr = p;
}



void SecPointerBase::copy(SecCFObject * p)
{
	if (ptr)
	{
		CheckForRelease(ptr);
		CFRelease(ptr->operator CFTypeRef());
	}
	
	ptr = p;
}



//
// SecCFObject
//
SecCFObject *
SecCFObject::optional(CFTypeRef cfTypeRef) throw()
{
	if (!cfTypeRef)
		return NULL;

	return const_cast<SecCFObject *>(reinterpret_cast<const SecCFObject *>(reinterpret_cast<const uint8_t *>(cfTypeRef) + kAlignedRuntimeSize));
}

SecCFObject *
SecCFObject::required(CFTypeRef cfTypeRef, OSStatus error)
{
	SecCFObject *object = optional(cfTypeRef);
	if (!object)
		MacOSError::throwMe(error);

	return object;
}

void *
SecCFObject::allocate(size_t size, const CFClass &cfclass) throw(std::bad_alloc)
{
	CFTypeRef p = _CFRuntimeCreateInstance(NULL, cfclass.typeID,
		size + kAlignedRuntimeSize - sizeof(CFRuntimeBase), NULL);
	if (p == NULL)
		throw std::bad_alloc();

	((SecRuntimeBase*) p)->isNew = true;

	void *q = ((u_int8_t*) p) + kAlignedRuntimeSize;

	if (SECURITY_DEBUG_SEC_CREATE_ENABLED()) {
		const CFRuntimeClass *rtc = _CFRuntimeGetClassWithTypeID(cfclass.typeID);
		SECURITY_DEBUG_SEC_CREATE(q, rtc ? (char *)rtc->className : NULL, (unsigned int)cfclass.typeID);
	}
	return q;
}

void
SecCFObject::operator delete(void *object) throw()
{
	CFTypeRef cfType = reinterpret_cast<CFTypeRef>(reinterpret_cast<const uint8_t *>(object) - kAlignedRuntimeSize);
    if (CF_IS_COLLECTABLE(cfType))
    {
        return;
    }
    
    CFAllocatorRef allocator = CFGetAllocator(cfType);
    CFAllocatorDeallocate(allocator, (void*) cfType);
}

SecCFObject::SecCFObject()
{
    mRetainCount = 1;
    mRetainSpinLock = OS_SPINLOCK_INIT;
}

uint32_t SecCFObject::updateRetainCount(intptr_t direction, uint32_t *oldCount)
{
    OSSpinLockLock(&mRetainSpinLock);

    if (oldCount != NULL)
    {
        *oldCount = mRetainCount;
    }
    
    if (direction != -1 || mRetainCount != 0)
    {
        // if we are decrementing
        if (direction == -1 || UINT32_MAX != mRetainCount)
        {
            mRetainCount += direction;
        }
    }
    
    uint32_t result = mRetainCount;

    OSSpinLockUnlock(&mRetainSpinLock);
    
    return result;
}



SecCFObject::~SecCFObject()
{
	SECURITY_DEBUG_SEC_DESTROY(this);
}

bool
SecCFObject::equal(SecCFObject &other)
{
	return this == &other;
}

CFHashCode
SecCFObject::hash()
{
	return CFHashCode(this);
}

CFStringRef
SecCFObject::copyFormattingDesc(CFDictionaryRef dict)
{
	return NULL;
}

CFStringRef
SecCFObject::copyDebugDesc()
{
	return NULL;
}

CFTypeRef
SecCFObject::handle(bool retain) throw()
{
	CFTypeRef cfType = *this;
	if (retain && !isNew()) CFRetain(cfType);
	return cfType;
}



void
SecCFObject::aboutToDestruct()
{
}



Mutex*
SecCFObject::getMutexForObject()
{
	return NULL; // we only worry about descendants of KeychainImpl and ItemImpl
}



bool SecCFObject::mayDelete()
{
    return true;
}
