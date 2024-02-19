#pragma once

template <typename T>
class Singleton
{
  private:
    static T* ptrInst;

  public:
    template <typename... Args>
    static auto Inst(Args&... args) -> T&
    {
        if (!static_cast<bool>(ptrInst))
        {
            ptrInst = new T(args...);
        }
        return *ptrInst;
    }

    Singleton(const Singleton&)                     = delete;
    Singleton(Singleton&&)                          = delete;
    auto operator= (const Singleton&) -> Singleton& = delete;
    auto operator= (Singleton&&) -> Singleton&      = delete;

  protected:
    Singleton()  = default;
    ~Singleton() = default;
};

template <typename T>
inline T* Singleton<T>::ptrInst = nullptr;
