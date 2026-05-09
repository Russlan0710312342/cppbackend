#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io},
        gas_cooker_(std::make_shared<GasCooker>(io_)) {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        struct State {
            std::shared_ptr<Sausage> sausage;
            std::shared_ptr<Bread> bread;
            std::atomic_bool sausage_ready{false};
            std::atomic_bool bread_ready{false};
        };

        auto state = std::make_shared<State>();
        state->sausage = sausage;
        state->bread = bread;

        auto try_finish = [this, state, handler]() {
            if (state->sausage_ready.load() && state->bread_ready.load()) {
                try {
                    HotDog hotdog(++next_hotdog_id_, state->sausage, state->bread);
                    handler(Result<HotDog>(std::move(hotdog)));
                } catch (...) {
                    handler(Result<HotDog>::FromCurrentException());
                }
            }
        };

        gas_cooker_->UseBurner([this, sausage, state, try_finish]() {
            sausage->StartFry(*gas_cooker_, [this, sausage, state, try_finish]() {

                auto timer = std::make_shared<net::steady_timer>(
                    io_, HotDog::MIN_SAUSAGE_COOK_DURATION);

                timer->async_wait([this, sausage, state, try_finish, timer](sys::error_code ec) {
                    if (ec) return;

                    sausage->StopFry();

                    gas_cooker_->ReleaseBurner();

                    state->sausage_ready = true;
                    try_finish();
                });
            });
        });

        gas_cooker_->UseBurner([this, bread, state, try_finish]() {
            bread->StartBake(*gas_cooker_, [this, bread, state, try_finish]() {

                auto timer = std::make_shared<net::steady_timer>(
                    io_, HotDog::MIN_BREAD_COOK_DURATION);

                timer->async_wait([this, bread, state, try_finish, timer](sys::error_code ec) {
                    if (ec) return;

                    bread->StopBaking();

                    gas_cooker_->ReleaseBurner();

                    state->bread_ready = true;
                    try_finish();
                });
            });
        });
    }
private:
    net::io_context& io_;

    Store store_;

    std::shared_ptr<GasCooker> gas_cooker_;

    std::atomic_int next_hotdog_id_ = 0;
};