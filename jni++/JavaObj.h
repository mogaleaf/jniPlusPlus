#pragma once
#include "JavaClass.h"
#include <string>
#include <functional>


class JavaObj : public JavaClass {
    jobject objId;
    std::map<std::string, jmethodID> methodCache;


public:
    JavaObj() : JavaClass() {
    }

    JavaObj(std::string classPath) : JavaClass(classPath), objId(nullptr) {
    }

    JavaObj(std::string classPath, jobject objId, JNIEnv* env) : JavaClass(classPath, env) {
        this->objId = objId;
    }

    std::string getSignature() const {
        return "L" + classPath + ";";
    }

    std::string getClassPath() const {
        return classPath;
    }

    jobject getObjId() const {
        return objId;
    }

    /*    template <typename... Args>
        auto getVoidMethod(std::string name, Args&&... args) {
            auto methodId = getVoidMethodID(name, args...);
            return std::bind(&JavaObj::callVoid<Args...>, this, methodId, args...);
        }
        */
    template <typename ReturnType, typename... Args>
    ReturnType call(std::string name, ReturnType returnType, Args&&... args) {
        auto methodId = getMethodID(name, returnType, args...);
        checkExceptions("JavaObj.call GetMethodId");
        return call(methodId, returnType, args...);
    }

    template <typename ReturnType, typename... Args>
    ReturnType call(jmethodID methodId, const ReturnType& returnType, Args&&... args) {
        auto jvalues = createJValues(args...);
        auto result = callMethod(methodId, returnType, jvalues.get());
        checkExceptions("JavaObj.call callMethod");
        return result;
    }

    template <typename... Args>
    void callVoid(std::string name, Args&&... args) {
        callVoid(getVoidMethodID(name, args...), args...);
    }

    template <typename... Args>
    void callVoid(jmethodID methodId, Args&&... args) {
        checkExceptions("JavaObj.callVoid GetMethodId");
        auto jvalues = createJValues(args...);
        callVoidMethod(methodId, jvalues.get());
        checkExceptions("JavaObj.callVoid callVoidMethod");
    }


    template <typename ReturnType, typename... Args>
    jmethodID getMethodID(std::string methodName, const ReturnType& returnType, Args&&... args) {
        return getMethodID(methodName, signature(returnType, args...));
    }

    template <typename... Args>
    jmethodID getVoidMethodID(std::string methodName, Args&&... args) {
        return getMethodID(methodName, voidSignature(args...));
    }



    template <typename BufferType>
    auto createDirectBuffer(const BufferType& buffer) {
        auto javaByteBuffer = JavaObj("java.nio.ByteBuffer", env->NewDirectByteBuffer(const_cast<void*>(reinterpret_cast<const void*>(buffer.data())), buffer.size() * sizeof(BufferType::value_type)), env);
        checkExceptions("linkBuffer NewDirectByteBuffer");

        auto byteNativeOrder = JavaClass("java.nio.ByteOrder", env).call("nativeOrder", JavaObj("java.nio.ByteOrder"));
        javaByteBuffer.call("order", JavaObj("java.nio.ByteBuffer"), byteNativeOrder);

        return JavaObj("java.nio.Buffer", convertBufferType<typename BufferType::value_type>(javaByteBuffer.getObjId()), env);
    }

    template <typename BufferType>
    void linkBuffer(std::string fieldName, const BufferType& buffer) {
        auto bufferId = env->GetFieldID(classId, fieldName.c_str(), getBufferName<typename BufferType::value_type>("Ljava/nio/", ";").c_str());
        checkExceptions("linkBuffer GetFieldId");
        
        auto javaBuffer = createDirectBuffer(buffer);
        env->SetObjectField(objId, bufferId, javaBuffer.getObjId());
        checkExceptions("linkBuffer SetObjectField");
    }

private:
    template <typename DataType>
    std::string getBufferName(std::string prolog, std::string punct) {
        return prolog + "ByteBuffer" + punct;
    }

    template <typename DataType>
    static std::string getBufferMethodName() {
        return "";
    }

    template <typename DataType>
    auto convertBufferType(jobject buffer) {
        return buffer;
    }

    jmethodID getMethodID(std::string methodName, std::string signature) {
        try {
            return methodCache.at(methodName);
        }
        catch (std::out_of_range) {
            auto methodId = env->GetMethodID(classId, methodName.c_str(), signature.c_str());
            methodCache[methodName] = methodId;
            return methodId;
        }
    }

    template <typename ReturnType>
    ReturnType callMethod(jmethodID methodId, const ReturnType& returnType, jvalue* args) const {
        return ReturnType(returnType.getClassPath(), callMethod(methodId, jobject(), args), env);
    };

    void callVoidMethod(jmethodID methodId, jvalue* args) const {
        env->CallVoidMethodA(objId, methodId, args);
    }

    template <typename DataType>
    auto convertBufferTypeHelper(jobject buffer) {
        auto asTypeBuffer = env->GetMethodID(env->GetObjectClass(buffer), getBufferName<DataType>("as", "").c_str(), getBufferName<DataType>("()Ljava/nio/", ";").c_str());
        checkExceptions("convertBufferTypeHelper GetMethodID");
        auto convertedBuffer = env->CallObjectMethod(buffer, asTypeBuffer);
        checkExceptions("convertBufferTypeHelper CallObjectMethod");
        return convertedBuffer;
    }
};

template <>
inline std::string JavaObj::getBufferName<uint16_t>(std::string prolog, std::string punct) {
    return prolog + "ShortBuffer" + punct;
}

template <>
inline std::string JavaObj::getBufferName<uint32_t>(std::string prolog, std::string punct) {
    return prolog + "IntBuffer" + punct;
}

template <>
inline std::string JavaObj::getBufferName<uint64_t>(std::string prolog, std::string punct) {
    return prolog + "LongBuffer" + punct;
}


template <>
inline auto JavaObj::convertBufferType<uint16_t>(jobject buffer) {
    return convertBufferTypeHelper<uint16_t>(buffer);
}

template <>
inline auto JavaObj::convertBufferType<uint32_t>(jobject buffer) {
    return convertBufferTypeHelper<uint32_t>(buffer);
}

template <>
inline auto JavaObj::convertBufferType<uint64_t>(jobject buffer) {
    return convertBufferTypeHelper<uint64_t>(buffer);
}


template <>
inline jobject JavaObj::callMethod(jmethodID methodId, const jobject&, jvalue* args) const {
    return env->CallObjectMethodA(objId, methodId, args);
}

template <>
inline bool JavaObj::callMethod(jmethodID methodId, const bool&, jvalue* args) const {
    return env->CallBooleanMethodA(objId, methodId, args) != 0;
}

template <>
inline uint8_t JavaObj::callMethod(jmethodID methodId, const uint8_t&, jvalue* args) const {
    return env->CallByteMethodA(objId, methodId, args);
}

template <>
inline int8_t JavaObj::callMethod(jmethodID methodId, const int8_t&, jvalue* args) const {
    return static_cast<char>(env->CallCharMethodA(objId, methodId, args));
}

template <>
inline int16_t JavaObj::callMethod(jmethodID methodId, const int16_t&, jvalue* args) const {
    return env->CallShortMethodA(objId, methodId, args);
}

template <>
inline uint16_t JavaObj::callMethod(jmethodID methodId, const uint16_t&, jvalue* args) const {
    return static_cast<uint16_t>(env->CallIntMethodA(objId, methodId, args));
}

template <>
inline int32_t JavaObj::callMethod(jmethodID methodId, const int32_t&, jvalue* args) const {
    return env->CallIntMethodA(objId, methodId, args);
}

template <>
inline uint32_t JavaObj::callMethod(jmethodID methodId, const uint32_t&, jvalue* args) const {
    return static_cast<uint32_t>(env->CallLongMethodA(objId, methodId, args));
}

template <>
inline int64_t JavaObj::callMethod(jmethodID methodId, const int64_t&, jvalue* args) const {
    return env->CallLongMethodA(objId, methodId, args);
}

template <>
inline float JavaObj::callMethod(jmethodID methodId, const float&, jvalue* args) const {
    return env->CallFloatMethodA(objId, methodId, args);
}

template <>
inline double JavaObj::callMethod(jmethodID methodId, const double&, jvalue* args) const {
    return env->CallDoubleMethodA(objId, methodId, args);
}

template <>
inline std::string JavaClass::getSymbol(const JavaObj& obj) {
    return obj.getSignature();
}

template <>
inline jvalue JavaClass::toJvalue(const JavaObj& v) {
    jvalue j;
    j.l = v.getObjId();
    return j;
}

