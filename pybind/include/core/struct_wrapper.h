#ifndef CHIAKI_PY_STRUCT_WRAPPER_H
#define CHIAKI_PY_STRUCT_WRAPPER_H

template <typename T>
class StructWrapper
{
private:
    T raw_struct;
public:
    StructWrapper() : raw_struct{} {}
    StructWrapper(const T &raw) : raw_struct(raw) {}
    StructWrapper(T &&raw) : raw_struct(std::move(raw)) {}

    // Operator overloads
    T *operator->() { return &raw_struct; }
    const T *operator->() const { return &raw_struct; }

    T &operator*() { return raw_struct; }
    const T &operator*() const { return raw_struct; }

    operator T *() { return &raw_struct; }
    operator const T *() const { return &raw_struct; }

    T *ptr() { return &raw_struct; }
    T &raw() { return raw_struct; }
    const T &raw() const { return raw_struct; }
};

#endif // CHIAKI_PY_STRUCT_WRAPPER_H