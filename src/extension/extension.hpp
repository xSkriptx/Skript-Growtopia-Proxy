#pragma once
#include <unordered_map>
#include <ranges>
#include <string>
#include <functional>






#define PROVIDE_EXT_UID(uid)									\
	static constexpr std::size_t ExtensionUID = uid;			\
	std::size_t get_uid() override { return ExtensionUID; }

namespace extension {






struct IExtension {
	IExtension() = default;
	virtual ~IExtension()
	{
		for (const auto& method : std::views::values(methods_)) {
			::operator delete(method);
		}
	}

	




	virtual std::size_t get_uid() = 0;

	



	virtual void init() = 0;

	



	virtual void tick() { }

	



	virtual void free() = 0;

public:
	






	template<typename Callback>
	void add_callable_method(const std::string& name, Callback cb)
	{
		auto f{ to_function(cb) };
		auto fn{ new decltype(f)(f) };
		methods_.emplace(name, fn);
	}

	








	template <typename R, typename... Args>
	R call_method(const std::string& name, Args... args)
	{
		const auto it{ methods_.find(name) };
		if (it == methods_.end()) {
			return R{};
		}

		auto fn = static_cast<std::function<R(Args...)>*>(it->second);
		return (*fn)(args...);
	}

private:
	




	template <typename Callback>
	struct traits : traits<decltype(&Callback::operator())> {

	};

	





	template <typename R, typename... Args>
	struct traits<R(*)(Args...)> {
		using fn = std::function<R(Args...)>;
	};

	






	template <typename ClassType, typename R, typename... Args>
	struct traits<R(ClassType::*)(Args...) const> {
		using fn = std::function<R(Args...)>;
	};

	





	template <typename R, typename... Args>
	struct traits<std::function<R(Args...)>> {
		using fn = std::function<R(Args...)>;
	};

	






	template <typename R, typename ClassType, typename... Args>
#ifdef _MSC_VER 
	struct traits<std::_Binder<std::_Unforced, R(ClassType::*)(Args...) const, ClassType*>> {
#else 
	struct traits<std::_Bind<R(ClassType::*)(Args...) const>> {
#endif
		using fn = std::function<R(Args...)>;
	};

	




	template <typename Callback>
	typename traits<Callback>::fn to_function(Callback& cb) {
		return static_cast<typename traits<Callback>::fn>(cb);
	}

private:
    std::unordered_map<std::string, void*> methods_;
};







class Extensible {
public:
	virtual ~Extensible() { remove_extensions(); }

	






	virtual bool add_extension(IExtension* ext)
	{
		if (!ext) {
			return false;
		}

		const auto it{ extensions_.find(ext->get_uid()) };
		if (it != extensions_.end() && it->second != ext) {
			remove_extension(ext->get_uid());
		}

		return extensions_.emplace(ext->get_uid(), ext).second;
	}

	





	virtual IExtension* get_extension(const std::size_t id)
	{
		const auto it{ extensions_.find(id) };
		if (it == extensions_.end()) {
			return nullptr;
		}

		return it->second;
	}

	




	template <class T>
	T* query_extension()
	{
		static_assert(std::is_base_of_v<IExtension, T>, "Type must derive from IExtension");

		if (IExtension* extension{ get_extension(T::ExtensionUID) }) {
			return static_cast<T*>(extension);
		}

		return nullptr;
	}

	






	virtual bool remove_extension(IExtension* ext)
	{
		const auto it{ extensions_.find(ext->get_uid()) };
		if (it == extensions_.end()) {
			return false;
		}

		if (it->second) {
			it->second->free();
		}

		extensions_.erase(it);
		return true;
	}

	






	virtual bool remove_extension(const std::size_t id)
	{
		const auto it{ extensions_.find(id) };
		if (it == extensions_.end()) {
			return false;
		}

		if (it->second) {
			it->second->free();
		}

		extensions_.erase(it);
		return true;
	}

private:
	void remove_extensions()
	{
		for (const auto& ext : std::views::values(extensions_)) {
			remove_extension(ext);
		}
	}

protected:
	std::unordered_map<std::size_t, IExtension*> extensions_;
};
}
