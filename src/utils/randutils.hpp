




























#ifndef RANDUTILS_HPP
#define RANDUTILS_HPP 1



























































#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <array>
#include <functional>  
#include <initializer_list>
#include <utility>
#include <type_traits>
#include <iterator>
#include <chrono>
#include <thread>
#include <algorithm>



#if !defined(RANDUTILS_CPU_ENTROPY) && defined(__has_builtin)
    #if __has_builtin(__builtin_readcyclecounter) && !defined(__aarch64__)
        #define RANDUTILS_CPU_ENTROPY __builtin_readcyclecounter()
    #endif
#endif
#if !defined(RANDUTILS_CPU_ENTROPY)
    #if __i386__
        #if __GNUC__
            #define RANDUTILS_CPU_ENTROPY __builtin_ia32_rdtsc()
        #else
            #include <immintrin.h>
            #define RANDUTILS_CPU_ENTROPY __rdtsc()
        #endif
    #else
        #define RANDUTILS_CPU_ENTROPY 0
    #endif
#endif

#if defined(RANDUTILS_GETPID)
    
#elif defined(_WIN64) || defined(_WIN32)
    #include <process.h>
    #define RANDUTILS_GETPID _getpid()
#elif defined(__unix__) || defined(__unix) \
      || (defined(__APPLE__) && defined(__MACH__))
    #include <unistd.h>
    #define RANDUTILS_GETPID getpid()
#else
    #define RANDUTILS_GETPID 0
#endif

#if __cpp_constexpr >= 201304L
    #define RANDUTILS_GENERALIZED_CONSTEXPR constexpr
#else
    #define RANDUTILS_GENERALIZED_CONSTEXPR
#endif



namespace randutils {




































































template <size_t count = 4, typename IntRep = uint32_t,
          size_t mix_rounds = 1 + (count <= 2)>
struct seed_seq_fe {
public:
    
    typedef IntRep result_type;

private:
    static constexpr uint32_t INIT_A = 0x43b0d7e5;
    static constexpr uint32_t MULT_A = 0x931e8875;

    static constexpr uint32_t INIT_B = 0x8b51f9dd;
    static constexpr uint32_t MULT_B = 0x58f38ded;

    static constexpr uint32_t MIX_MULT_L = 0xca01f9dd;
    static constexpr uint32_t MIX_MULT_R = 0x4973f715;
    static constexpr uint32_t XSHIFT = sizeof(IntRep)*8/2;

    RANDUTILS_GENERALIZED_CONSTEXPR
    static IntRep fast_exp(IntRep x, IntRep power)
    {
        IntRep result = IntRep(1);
        IntRep multiplier = x;
        while (power != IntRep(0)) {
            IntRep thismult = power & IntRep(1) ? multiplier : IntRep(1);
            result *= thismult;
            power >>= 1;
            multiplier *= multiplier;
        }
        return result;
    }

    std::array<IntRep, count> mixer_;

    template <typename InputIter>
    void mix_entropy(InputIter begin, InputIter end);

public:
    seed_seq_fe(const seed_seq_fe&)     = delete;
    void operator=(const seed_seq_fe&)  = delete;

    template <typename T>
    seed_seq_fe(std::initializer_list<T> init)
    {
        seed(init.begin(), init.end());
    }

    template <typename InputIter>
    seed_seq_fe(InputIter begin, InputIter end)
    {
        seed(begin, end);
    }

    
    template <typename RandomAccessIterator>
    void generate(RandomAccessIterator first, RandomAccessIterator last) const;

    static constexpr size_t size()
    {
        return count;
    }

    template <typename OutputIterator>
    void param(OutputIterator dest) const;

    template <typename InputIter>
    void seed(InputIter begin, InputIter end)
    {
        mix_entropy(begin, end);
        
        
        for (size_t i = 1; i < mix_rounds; ++i)
            stir();
    }

    seed_seq_fe& stir()
    {
        mix_entropy(mixer_.begin(), mixer_.end());
        return *this;
    }

};

template <size_t count, typename IntRep, size_t r>
template <typename InputIter>
void seed_seq_fe<count, IntRep, r>::mix_entropy(InputIter begin, InputIter end)
{
    auto hash_const = INIT_A;
    auto hash = [&](IntRep value) {
        value ^= hash_const;
        hash_const *= MULT_A;
        value *= hash_const;
        value ^= value >> XSHIFT;
        return value;
    };
    auto mix = [](IntRep x, IntRep y) {
        IntRep result = MIX_MULT_L*x - MIX_MULT_R*y;
        result ^= result >> XSHIFT;
        return result;
    };

    InputIter current = begin;
    for (auto& elem : mixer_) {
        if (current != end)
            elem = hash(*current++);
        else
            elem = hash(0U);
    }
    for (auto& src : mixer_)
        for (auto& dest : mixer_)
            if (&src != &dest)
                dest = mix(dest,hash(src));
    for (; current != end; ++current)
        for (auto& dest : mixer_)
            dest = mix(dest,hash(*current));
}

template <size_t count, typename IntRep, size_t mix_rounds>
template <typename OutputIterator>
void seed_seq_fe<count,IntRep,mix_rounds>::param(OutputIterator dest) const
{
    const IntRep INV_A = fast_exp(MULT_A, IntRep(-1));
    const IntRep MIX_INV_L = fast_exp(MIX_MULT_L, IntRep(-1));

    auto mixer_copy = mixer_;
    for (size_t round = 0; round < mix_rounds; ++round) {
        
        auto hash_const = INIT_A*fast_exp(MULT_A, IntRep(count * count));

        for (auto src = mixer_copy.rbegin(); src != mixer_copy.rend(); ++src)
            for (auto dest = mixer_copy.rbegin(); dest != mixer_copy.rend();
                 ++dest)
                if (src != dest) {
                    IntRep revhashed = *src;
                    auto mult_const = hash_const;
                    hash_const *= INV_A;
                    revhashed ^= hash_const;
                    revhashed *= mult_const;
                    revhashed ^= revhashed >> XSHIFT;
                    IntRep unmixed = *dest;
                    unmixed ^= unmixed >> XSHIFT;
                    unmixed += MIX_MULT_R*revhashed;
                    unmixed *= MIX_INV_L;
                    *dest = unmixed;
                }
        for (auto i = mixer_copy.rbegin(); i != mixer_copy.rend(); ++i) {
            IntRep unhashed = *i;
            unhashed ^= unhashed >> XSHIFT;
            unhashed *= fast_exp(hash_const, IntRep(-1));
            hash_const *= INV_A;
            unhashed ^= hash_const;
            *i = unhashed;
        }
    }
    std::copy(mixer_copy.begin(), mixer_copy.end(), dest);
}


template <size_t count, typename IntRep, size_t mix_rounds>
template <typename RandomAccessIterator>
void seed_seq_fe<count,IntRep,mix_rounds>::generate(
        RandomAccessIterator dest_begin,
        RandomAccessIterator dest_end) const
{
    auto src_begin = mixer_.begin();
    auto src_end   = mixer_.end();
    auto src       = src_begin;
    auto hash_const = INIT_B;
    for (auto dest = dest_begin; dest != dest_end; ++dest) {
        auto dataval = *src;
        if (++src == src_end)
            src = src_begin;
        dataval ^= hash_const;
        hash_const *= MULT_B;
        dataval *= hash_const;
        dataval ^= dataval >> XSHIFT;
        *dest = dataval;
    }
}

using seed_seq_fe128 = seed_seq_fe<4, uint32_t>;
using seed_seq_fe256 = seed_seq_fe<8, uint32_t>;



























template <typename SeedSeq>
class auto_seeded : public SeedSeq {
    using default_seeds = std::array<uint32_t, 13>;

    template <typename T>
    static uint32_t crushto32(T value)
    {
        if (sizeof(T) <= 4)
            return uint32_t(value);
        else {
            uint64_t result = uint64_t(value);
            result *= 0xbc2ad017d719504d;
            return uint32_t(result ^ (result >> 32));
        }
    }

    template <typename T>
    static uint32_t hash(T&& value)
    {
        return crushto32(std::hash<typename std::remove_reference<
                                    typename std::remove_cv<T>::type>::type>{}(
                                     std::forward<T>(value)));
    }

    static constexpr uint32_t fnv(uint32_t hash, const char* pos)
    {
        return *pos == '\0' ? hash : fnv((hash * 16777619U) ^ *pos, pos+1);
    }

    default_seeds local_entropy()
    {
        
        constexpr uint32_t compile_stamp =
            fnv(2166136261U, __DATE__ __TIME__ __FILE__);

        
        
        
        static uint32_t random_int = std::random_device{}();

        
        void* malloc_addr = malloc(sizeof(int));
        free(malloc_addr);
        auto heap  = hash(malloc_addr);
        auto stack = hash(&malloc_addr);

        
        
        random_int += 0xedf19156;

        
        
        auto hitime = std::chrono::high_resolution_clock::now()
                        .time_since_epoch().count();

        
        
        
        
        
        auto self_data = hash(this);

        
        
        
        auto time_func = hash(&std::chrono::high_resolution_clock::now);

        
        
        
        
        auto exit_func = hash(&_Exit);

        
        
        
        
        
        auto self_func = hash(
            static_cast<uint32_t (*)(uint64_t)>(
                                &auto_seeded::crushto32));

        
        
        auto thread_id  = hash(std::this_thread::get_id());

        
        
        #if __cpp_rtti || __GXX_RTTI
        auto type_id   = crushto32(typeid(*this).hash_code());
        #else
        uint32_t type_id   = 0;
        #endif

        
        auto pid = crushto32(RANDUTILS_GETPID);
        auto cpu = crushto32(RANDUTILS_CPU_ENTROPY);

        return {{random_int, crushto32(hitime), stack, heap, self_data,
                 self_func, exit_func, time_func, thread_id, type_id, pid,
                 cpu, compile_stamp}};
    }


public:
    using SeedSeq::SeedSeq;

    using base_seed_seq = SeedSeq;

    const base_seed_seq& base() const
    {
        return *this;
    }

    base_seed_seq& base()
    {
        return *this;
    }

    auto_seeded(default_seeds seeds)
        : SeedSeq(seeds.begin(), seeds.end())
    {
        
    }

    auto_seeded()
        : auto_seeded(local_entropy())
    {
        
    }
};

using auto_seed_128 = auto_seeded<seed_seq_fe128>;
using auto_seed_256 = auto_seeded<seed_seq_fe256>;















template <typename Numeric>
using uniform_distribution = typename std::conditional<
            std::is_integral<Numeric>::value,
              std::uniform_int_distribution<Numeric>,
              std::uniform_real_distribution<Numeric> >::type;



























template <typename RandomEngine = std::default_random_engine,
          typename DefaultSeedSeq = auto_seed_256>
class random_generator {
public:
    using engine_type       = RandomEngine;
    using default_seed_type = DefaultSeedSeq;
private:
    engine_type engine_;

    
    
    
    
    

    template <typename T>
    static constexpr bool has_base_seed_seq(typename T::base_seed_seq*)
    {
        return true;
    }

    template <typename T>
    static constexpr bool has_base_seed_seq(...)
    {
        return false;
    }


    template <typename SeedSeqBased>
    static auto seed_seq_cast(SeedSeqBased&& seq,
                               typename std::enable_if<
                                 has_base_seed_seq<SeedSeqBased>(0)>::type* = 0)
                                        -> decltype(seq.base())
    {
        return seq.base();
    }

    template <typename SeedSeq>
    static SeedSeq seed_seq_cast(SeedSeq&& seq,
                                   typename std::enable_if<
                                     !has_base_seed_seq<SeedSeq>(0)>::type* = 0)
    {
        return seq;
    }

public:
    template <typename Seeding = default_seed_type,
              typename... Params>
    random_generator(Seeding&& seeding = default_seed_type{})
        : engine_{seed_seq_cast(std::forward<Seeding>(seeding))}
    {
        
    }

    
    
    
    
    template <typename Seeding,
              typename... Params>
    random_generator(Seeding&& seeding, Params&&... params)
        : engine_{seed_seq_cast(std::forward<Seeding>(seeding)),
                  std::forward<Params>(params)...}
    {
        
    }

    template <typename Seeding = default_seed_type,
              typename... Params>
    void seed(Seeding&& seeding = default_seed_type{})
    {
        engine_.seed(seed_seq_cast(seeding));
    }

    
    
    
    
    template <typename Seeding,
              typename... Params>
    void seed(Seeding&& seeding, Params&&... params)
    {
        engine_.seed(seed_seq_cast(seeding), std::forward<Params>(params)...);
    }


    RandomEngine& engine()
    {
        return engine_;
    }

    template <typename ResultType,
              template <typename> class DistTmpl = std::normal_distribution,
              typename... Params>
    ResultType variate(Params&&... params)
    {
        DistTmpl<ResultType> dist(std::forward<Params>(params)...);

        return dist(engine_);
    }

    template <typename Numeric>
    Numeric uniform(Numeric lower, Numeric upper)
    {
        return variate<Numeric,uniform_distribution>(lower, upper);
    }

    template <template <typename> class DistTmpl = uniform_distribution,
              typename Iter,
              typename... Params>
    void generate(Iter first, Iter last, Params&&... params)
    {
        using result_type =
           typename std::remove_reference<decltype(*(first))>::type;

        DistTmpl<result_type> dist(std::forward<Params>(params)...);

        std::generate(first, last, [&]{ return dist(engine_); });
    }

    template <template <typename> class DistTmpl = uniform_distribution,
              typename Range,
              typename... Params>
    void generate(Range&& range, Params&&... params)
    {
        generate<DistTmpl>(std::begin(range), std::end(range),
                           std::forward<Params>(params)...);
    }

    template <typename Iter>
    void shuffle(Iter first, Iter last)
    {
        std::shuffle(first, last, engine_);
    }

    template <typename Range>
    void shuffle(Range&& range)
    {
        shuffle(std::begin(range), std::end(range));
    }


    template <typename Iter>
    Iter choose(Iter first, Iter last)
    {
        auto dist = std::distance(first, last);
        if (dist < 2)
            return first;
        using distance_type = decltype(dist);
        distance_type choice = uniform(distance_type(0), --dist);
        std::advance(first, choice);
        return first;
    }

    template <typename Range>
    auto choose(Range&& range) -> decltype(std::begin(range))
    {
        return choose(std::begin(range), std::end(range));
    }


    template <typename Range>
    auto pick(Range&& range) -> decltype(*std::begin(range))
    {
        return *choose(std::begin(range), std::end(range));
    }

    template <typename T>
    auto pick(std::initializer_list<T> range) -> decltype(*range.begin())
    {
        return *choose(range.begin(), range.end());
    }


    template <typename Size, typename Iter>
    Iter sample(Size to_go, Iter first, Iter last)
    {
        auto total = std::distance(first, last);
        using value_type = decltype(*first);

        return std::stable_partition(first, last,
             [&](const value_type&) {
                --total;
                using distance_type = decltype(total);
                distance_type zero{};
                if (uniform(zero, total) < to_go) {
                    --to_go;
                    return true;
                } else {
                    return false;
                }
             });
    }

    template <typename Size, typename Range>
    auto sample(Size to_go, Range&& range) -> decltype(std::begin(range))
    {
        return sample(to_go, std::begin(range), std::end(range));
    }
};

using default_rng = random_generator<std::default_random_engine>;
using mt19937_rng = random_generator<std::mt19937>;

}

#endif 