#pragma once

#include <jni++/JavaClass.h>
#include <jni++/JavaObj.h>
#include <jni++/JVM.h>

class JNI {
public:
	static JNI& getInstance() {
		static JNI instance;
		return instance;
	}

	JNI(JNI const&) = delete;
	JNI(JNI&&) = delete;
	
	JVM& getJVM(std::string classPath, bool verbose) {
		try {
			return *jvms.at(classPath).get();
		}
		catch (std::out_of_range) {
			auto jvm = std::make_unique<JVM>(classPath, verbose);
			auto jvmPtr = jvm.get();
			jvms[classPath] = std::move(jvm);
			return *jvmPtr;
		}
	}
private:
	JNI() = default;

	std::map<std::string, std::unique_ptr<JVM>> jvms;
};



