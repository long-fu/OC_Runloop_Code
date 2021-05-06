/*
 * Copyright (c) 2014 Apple Inc. All rights reserved.
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

/*	CFRuntime.h
	Copyright (c) 1999-2014, Apple Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFRUNTIME__)
#define __COREFOUNDATION_CFRUNTIME__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>
#include <stddef.h>

CF_EXTERN_C_BEGIN

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))

// GC: until we link against ObjC must use indirect functions.  Overridden in CFSetupFoundationBridging
CF_EXPORT bool kCFUseCollectableAllocator;
CF_EXPORT bool (*__CFObjCIsCollectable)(void *);

CF_INLINE Boolean _CFAllocatorIsSystemDefault(CFAllocatorRef allocator) {
    if (allocator == kCFAllocatorSystemDefault) return true;
    if (NULL == allocator || kCFAllocatorDefault == allocator) {
        return (kCFAllocatorSystemDefault == CFAllocatorGetDefault());
    }
    return false;
}

// is GC on?
#define CF_USING_COLLECTABLE_MEMORY (kCFUseCollectableAllocator)
// is GC on and is this the GC allocator?
#define CF_IS_COLLECTABLE_ALLOCATOR(allocator) (kCFUseCollectableAllocator && (NULL == (allocator) || kCFAllocatorSystemDefault == (allocator) || 0))
// is this allocated by the collector?
#define CF_IS_COLLECTABLE(obj) (__CFObjCIsCollectable ? __CFObjCIsCollectable((void*)obj) : false)

#else

#define kCFUseCollectableAllocator 0
#define __CFObjCIsCollectable 0

CF_INLINE Boolean _CFAllocatorIsSystemDefault(CFAllocatorRef allocator) {
    if (allocator == kCFAllocatorSystemDefault) return true;
    if (NULL == allocator || kCFAllocatorDefault == allocator) {
        return (kCFAllocatorSystemDefault == CFAllocatorGetDefault());
    }
    return false;
}

#define CF_USING_COLLECTABLE_MEMORY 0
#define CF_IS_COLLECTABLE_ALLOCATOR(allocator) 0
#define CF_IS_COLLECTABLE(obj) 0
#endif

enum {
    _kCFRuntimeNotATypeID = 0
};

enum { // Version field constants
    _kCFRuntimeScannedObject =     (1UL << 0),
    _kCFRuntimeResourcefulObject = (1UL << 2),  // tells CFRuntime to make use of the reclaim field
    _kCFRuntimeCustomRefCount =    (1UL << 3),  // tells CFRuntime to make use of the refcount field
    _kCFRuntimeRequiresAlignment = (1UL << 4),  // tells CFRuntime to make use of the requiredAlignment field
};
//MARK: CF -- RunTimme部分
typedef struct __CFRuntimeClass {
    CFIndex version;
    const char *className; // must be a pure ASCII string, nul-terminated
    void (*init)(CFTypeRef cf);
    CFTypeRef (*copy)(CFAllocatorRef allocator, CFTypeRef cf);
    void (*finalize)(CFTypeRef cf);
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyFormattingDesc)(CFTypeRef cf, CFDictionaryRef formatOptions);	// return str with retain
    CFStringRef (*copyDebugDesc)(CFTypeRef cf);	// return str with retain

// 获取类型 分配内存
#define CF_RECLAIM_AVAILABLE 1
    void (*reclaim)(CFTypeRef cf); // Or in _kCFRuntimeResourcefulObject in the .version to indicate this field should be used

// 内存引用计数 - 函数
#define CF_REFCOUNT_AVAILABLE 1
    uint32_t (*refcount)(intptr_t op, CFTypeRef cf); // Or in _kCFRuntimeCustomRefCount in the .version to indicate this field should be used
        // this field must be non-NULL when _kCFRuntimeCustomRefCount is in the .version field
        // - if the callback is passed 1 in 'op' it should increment the 'cf's reference count and return 0
        // - if the callback is passed 0 in 'op' it should return the 'cf's reference count, up to 32 bits
        // - if the callback is passed -1 in 'op' it should decrement the 'cf's reference count; if it is now zero, 'cf' should be cleaned up and deallocated (the finalize callback above will NOT be called unless the process is running under GC, and CF does not deallocate the memory for you; if running under GC, finalize should do the object tear-down and free the object memory); then return 0
        // remember to use saturation arithmetic logic and stop incrementing and decrementing when the ref count hits UINT32_MAX, or you will have a security bug
        // remember that reference count incrementing/decrementing must be done thread-safely/atomically
        // objects should be created/initialized with a custom ref-count of 1 by the class creation functions
        // do not attempt to use any bits within the CFRuntimeBase for your reference count; store that in some additional field in your CF object

		/*
		//当_kCFRuntimeCustomRefCount在.version字段中时，此字段必须为非NULL
        //-如果回调在'op'中传递1，则应增加'cf'的引用计数并返回0
        //-如果回调在'op'中传递0，则应返回'cf'的引用计数，最多32位

        //-如果回调在'op'中传递-1，则应减少'cf'的引用计数；
		如果现在为零，则应清理并释放'cf'
		（除非该进程在GC下运行，并且CF不会为您分配内存，否则不会调用上述finalize回调；
		如果在GC下运行，则finalize应该执行拆除对象并释放对象内存）；
		然后返回0
  
        //记住使用饱和度算术逻辑，并在ref计数达到UINT32_MAX时停止递增和递减，否则您将遇到安全漏洞
        //请记住，引用计数的递增/递减必须是线程安全/原子方式进行的
        //对象应由类创建函数使用自定义引用计数1创建/初始化
        //不要尝试将CFRuntimeBase中的任何位用于您的引用计数；将其存储在CF对象的其他字段中
		*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define CF_REQUIRED_ALIGNMENT_AVAILABLE 1
	// 结构体对其
    uintptr_t requiredAlignment; // Or in _kCFRuntimeRequiresAlignment in the .version field to indicate this field should be used; the allocator to _CFRuntimeCreateInstance() will be ignored in this case; if this is less than the minimum alignment the system supports, you'll get higher alignment; if this is not an alignment the system supports (e.g., most systems will only support powers of two, or if it is too high), the result (consequences) will be up to CF or the system to decide
    /*
	或在.version字段中的_kCFRuntimeRequiresAlignment中，指示应使用此字段； 
	在这种情况下，_CFRuntimeCreateInstance（）的分配器将被忽略； 
	如果这小于系统支持的最小对齐方式，则将获得更高的对齐方式； 
	如果这不是系统支持的对齐方式（例如，大多数系统仅支持2的幂，或者如果它太高），则结果（结果）将取决于CF或系统决定
	*/
} CFRuntimeClass;

#define RADAR_5115468_FIXED 1

/* Note that CF runtime class registration and unregistration is not currently
 * thread-safe, which should not currently be a problem, as long as unregistration
 * is done only when valid to do so.
 */

/*
/ *请注意，CF运行时类的注册和注销当前不是
  *线程安全，只要不注册，当前就不成问题
  *仅在有效时才这样做。
  */

CF_EXPORT CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass * const cls);
	/* Registers a new class with the CF runtime.  Pass in a
	 * pointer to a CFRuntimeClass structure.  The pointer is
	 * remembered by the CF runtime -- the structure is NOT
	 * copied.
	 *
	 * - version field must be zero currently.
	 * - className field points to a null-terminated C string
	 *   containing only ASCII (0 - 127) characters; this field
	 *   may NOT be NULL.
	 * - init field points to a function which classes can use to
	 *   apply some generic initialization to instances as they
	 *   are created; this function is called by both
	 *   _CFRuntimeCreateInstance and _CFRuntimeInitInstance; if
	 *   this field is NULL, no function is called; the instance
	 *   has been initialized enough that the polymorphic funcs
	 *   CFGetTypeID(), CFRetain(), CFRelease(), CFGetRetainCount(),
	 *   and CFGetAllocator() are valid on it when the init
	 *   function if any is called.
         * - copy field should always be NULL. Generic copying of CF
         *   objects has never been defined (and is unlikely).
	 * - finalize field points to a function which destroys an
	 *   instance when the retain count has fallen to zero; if
	 *   this is NULL, finalization does nothing. Note that if
	 *   the class-specific functions which create or initialize
	 *   instances more fully decide that a half-initialized
	 *   instance must be destroyed, the finalize function for
	 *   that class has to be able to deal with half-initialized
	 *   instances.  The finalize function should NOT destroy the
	 *   memory for the instance itself; that is done by the
	 *   CF runtime after this finalize callout returns.
	 * - equal field points to an equality-testing function; this
	 *   field may be NULL, in which case only pointer/reference
	 *   equality is performed on instances of this class. 
	 *   Pointer equality is tested, and the type IDs are checked
	 *   for equality, before this function is called (so, the
	 *   two instances are not pointer-equal but are of the same
	 *   class before this function is called).
	 * NOTE: the equal function must implement an immutable
	 *   equality relation, satisfying the reflexive, symmetric,
	 *    and transitive properties, and remains the same across
	 *   time and immutable operations (that is, if equal(A,B) at
	 *   some point, then later equal(A,B) provided neither
	 *   A or B has been mutated).
	 * - hash field points to a hash-code-computing function for
	 *   instances of this class; this field may be NULL in which
	 *   case the pointer value of an instance is converted into
	 *   a hash.
	 * NOTE: the hash function and equal function must satisfy
	 *   the relationship "equal(A,B) implies hash(A) == hash(B)";
	 *   that is, if two instances are equal, their hash codes must
	 *   be equal too. (However, the converse is not true!)
	 * - copyFormattingDesc field points to a function returning a
	 *   CFStringRef with a human-readable description of the
	 *   instance; if this is NULL, the type does not have special
	 *   human-readable string-formats.
	 * - copyDebugDesc field points to a function returning a
	 *   CFStringRef with a debugging description of the instance;
	 *   if this is NULL, a simple description is generated.
	 *
	 * This function returns _kCFRuntimeNotATypeID on failure, or
	 * on success, returns the CFTypeID for the new class.  This
	 * CFTypeID is what the class uses to allocate or initialize
	 * instances of the class. It is also returned from the
	 * conventional *GetTypeID() function, which returns the
	 * class's CFTypeID so that clients can compare the
	 * CFTypeID of instances with that of a class.
	 *
	 * The function to compute a human-readable string is very
	 * optional, and is really only interesting for classes,
	 * like strings or numbers, where it makes sense to format
	 * the instance using just its contents.
	 */

	/*
	/ *在CF运行时中注册一个新类。传递一个
*指向CFRuntimeClass结构的指针。指针是
*由CF运行时记住-结构不是
*复制。
*
*-version 字段当前必须为零。

*-className字段指向以N结尾的C字符串
*仅包含ASCII（0-127）字符；这个领域
*不能为NULL。

*-init 字段指向类可以使用的函数
*对实例应用一些通用的初始化
*已创建；这两个函数都被调用
* _CFRuntimeCreateInstance和_CFRuntimeInitInstance；如果
*该字段为NULL，不调用任何函数；实例
*已经足够初始化，以至于多态函数
* CFGetTypeID（），CFRetain（），CFRelease（），CFGetRetainCount（），
*和CFGetAllocator（）在初始化时对它有效
*函数（如果有的话）。
         *-copy 字段应始终为NULL。 CF的通用复制
         *对象从未被定义（并且是不可能的）。

*- finalize 确定一个指向破坏函数的字段
*保留计数降至零的实例；如果
*这是NULL，完成不做任何事情。请注意，如果
*创建或初始化的特定于类的函数
*实例更充分地决定将其初始化为一半
*实例必须销毁，finalize函数适用
*该类必须能够处理半初始化
*实例。 finalize函数不应破坏
*实例本身的内存；这是由
*此最终完成的调用返回后的CF运行时。

*-equal 字段指向相等性测试函数；这
*字段可以为NULL，在这种情况下，仅指针/引用
*在此类的实例上执行相等操作。
*测试指针相等性，并检查类型ID
*对于相等性，在调用此函数之前（因此，
*两个实例不是指针相等的，但具有相同的指针
*调用此函数之前的类）。
*注意：equal函数必须实现一个不可变的
*平等关系，满足反身，对称，
*和传递属性，并且在整个过程中保持不变
*时间和不变操作（即如果等于（A，B）
*有一点，后来等于（A，B）都不提供
* A或B已突变）。

*-hash 字段指向用于
*此类的实例；该字段可以为NULL，其中
*如果实例的指针值转换为
*哈希。
*注意：哈希函数和均等函数必须满足
*关系“ equal（A，B）意味着hash（A）== hash（B）”；
*也就是说，如果两个实例相等，则其哈希码必须
*也要平等。 （但是，事实并非如此！）

*-copyFormattingDesc字段指向一个返回a的函数
* CFStringRef带有人类可读的描述
*   实例;如果为NULL，则类型没有特殊
*人类可读的字符串格式。

*-copyDebugDesc字段指向一个返回a的函数
* CFStringRef带有实例的调试描述；
*如果为NULL，则生成一个简单的描述。
*
*此函数在失败时返回_kCFRuntimeNotATypeID，或者
*成功时，返回新类的CFTypeID。这
* CFTypeID是类用来分配或初始化的
*类的实例。它也从
*常规* GetTypeID（）函数，该函数返回
*类的CFTypeID，以便客户端可以比较
*具有类实例的CFTypeID。
*
*计算人类可读字符串的功能非常强大
*是可选的，并且实际上仅对类感兴趣，
*像字符串或数字一样，在格式上有意义
*仅使用实例内容的实例。
	*/

CF_EXPORT const CFRuntimeClass * _CFRuntimeGetClassWithTypeID(CFTypeID typeID);
	/* Returns the pointer to the CFRuntimeClass which was
	 * assigned the specified CFTypeID.
	 */

CF_EXPORT void _CFRuntimeUnregisterClassWithTypeID(CFTypeID typeID);
	/* Unregisters the class with the given type ID.  It is
	 * undefined whether type IDs are reused or not (expect
	 * that they will be).
	 *
	 * Whether or not unregistering the class is a good idea or
	 * not is not CF's responsibility.  In particular you must
	 * be quite sure all instances are gone, and there are no
	 * valid weak refs to such in other threads.
	 */

/* All CF "instances" start with this structure.  Never refer to
 * these fields directly -- they are for CF's use and may be added
 * to or removed or change format without warning.  Binary
 * compatibility for uses of this struct is not guaranteed from
 * release to release.
 */
typedef struct __CFRuntimeBase {
    uintptr_t _cfisa;
    uint8_t _cfinfo[4];
#if __LP64__
    uint32_t _rc;
#endif
} CFRuntimeBase;

#if __BIG_ENDIAN__
#define INIT_CFRUNTIME_BASE(...) {0, {0, 0, 0, 0x80}}
#else
#define INIT_CFRUNTIME_BASE(...) {0, {0x80, 0, 0, 0}}
#endif

CF_EXPORT CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category);
	/* Creates a new CF instance of the class specified by the
	 * given CFTypeID, using the given allocator, and returns it. 
	 * If the allocator returns NULL, this function returns NULL.
	 * A CFRuntimeBase structure is initialized at the beginning
	 * of the returned instance.  extraBytes is the additional
	 * number of bytes to allocate for the instance (BEYOND that
	 * needed for the CFRuntimeBase).  If the specified CFTypeID
	 * is unknown to the CF runtime, this function returns NULL.
	 * No part of the new memory other than base header is
	 * initialized (the extra bytes are not zeroed, for example).
	 * All instances created with this function must be destroyed
	 * only through use of the CFRelease() function -- instances
	 * must not be destroyed by using CFAllocatorDeallocate()
	 * directly, even in the initialization or creation functions
	 * of a class.  Pass NULL for the category parameter.
	 */

CF_EXPORT void _CFRuntimeSetInstanceTypeID(CFTypeRef cf, CFTypeID typeID);
	/* This function changes the typeID of the given instance.
	 * If the specified CFTypeID is unknown to the CF runtime,
	 * this function does nothing.  This function CANNOT be used
	 * to initialize an instance.  It is for advanced usages such
	 * as faulting. You cannot change the CFTypeID of an object
	 * of a _kCFRuntimeCustomRefCount class, or to a 
         * _kCFRuntimeCustomRefCount class.
	 */

CF_EXPORT void _CFRuntimeInitStaticInstance(void *memory, CFTypeID typeID);
	/* This function initializes a memory block to be a constant
	 * (unreleaseable) CF object of the given typeID.
	 * If the specified CFTypeID is unknown to the CF runtime,
	 * this function does nothing.  The memory block should
	 * be a chunk of in-binary writeable static memory, and at
	 * least as large as sizeof(CFRuntimeBase) on the platform
	 * the code is being compiled for.  The init function of the
	 * CFRuntimeClass is invoked on the memory as well, if the
	 * class has one. Static instances cannot be initialized to
	 * _kCFRuntimeCustomRefCount classes.
	 */
#define CF_HAS_INIT_STATIC_INSTANCE 1

CF_EXTERN_C_END

#endif /* ! __COREFOUNDATION_CFRUNTIME__ */

