#pragma once

namespace SudaGureum
{
    template<typename Arg>
    class Event final
    {
    public:
        Event() {}

    private:
        Event(const Event &) = delete;

    public:
        template<typename Func>
        boost::signals2::connection connect(Func fn, bool prior = false)
        {
            if(prior)
                return sig_.connect(0, std::move(fn));
            return sig_.connect(std::move(fn));
        }

        void disconnect(boost::signals2::connection conn)
        {
            return sig_.disconnect(conn);
        }

        void operator ()(Arg arg)
        {
            sig_(arg);
        }

    public:
        template<typename Func>
        boost::signals2::connection operator +=(Func rhs)
        {
            return sig_.connect(std::move(rhs));
        }

        void operator -=(boost::signals2::connection rhs)
        {
            return sig_.disconnect(rhs);
        }

    private:
        boost::signals2::signal<void(Arg)> sig_;
    };

    template<>
    class Event<void> final
    {
    public:
        Event() {}

    private:
        Event(const Event &) = delete;

    public:
        template<typename Func>
        boost::signals2::connection connect(Func fn, bool prior = false)
        {
            if(prior)
                return sig_.connect(0, std::move(fn));
            return sig_.connect(std::move(fn));
        }

        void disconnect(boost::signals2::connection conn)
        {
            return sig_.disconnect(conn);
        }

        void operator ()()
        {
            sig_();
        }

    public:
        template<typename Func>
        boost::signals2::connection operator +=(Func rhs)
        {
            return sig_.connect(std::move(rhs));
        }

        void operator -=(boost::signals2::connection rhs)
        {
            return sig_.disconnect(rhs);
        }

    private:
        boost::signals2::signal<void()> sig_;
    };
}
