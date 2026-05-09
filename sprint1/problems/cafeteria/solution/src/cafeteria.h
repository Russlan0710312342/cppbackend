#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;


class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронное приготовление хот-дога
    void OrderHotDog(HotDogHandler handler) {

        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        // Состояние заказа
        struct State {
            std::shared_ptr<Sausage> sausage;
            std::shared_ptr<Bread> bread;

            bool sausage_ready = false;
            bool bread_ready = false;
        };

        auto state = std::make_shared<State>();

        state->sausage = sausage;
        state->bread = bread;

        // Проверяем: готовы ли оба ингредиента
        auto check_ready = [state, handler]() {

            if (state->sausage_ready && state->bread_ready) {

                try {

                    HotDog hotdog(
                        state->sausage->GetId(),
                        state->sausage,
                        state->bread);

                    handler(Result<HotDog>(std::move(hotdog)));
                }
                catch (...) {

                    handler(Result<HotDog>::FromCurrentException());
                }
            }
        };

        // Готовим сосиску
        sausage->StartFry(
            *gas_cooker_,
            [this, sausage, state, check_ready]() {

                auto timer = std::make_shared<net::steady_timer>(
                    io_,
                    HotDog::MIN_SAUSAGE_COOK_DURATION);

                timer->async_wait(
                    [sausage, state, check_ready, timer](sys::error_code ec) {

                        if (ec) {
                            return;
                        }

                        sausage->StopFry();

                        state->sausage_ready = true;

                        check_ready();
                    });
            });

        // Готовим хлеб
        bread->StartBake(
            *gas_cooker_,
            [this, bread, state, check_ready]() {

                auto timer = std::make_shared<net::steady_timer>(
                    io_,
                    HotDog::MIN_BREAD_COOK_DURATION);

                timer->async_wait(
                    [bread, state, check_ready, timer](sys::error_code ec) {

                        if (ec) {
                            return;
                        }

                        bread->StopBaking();

                        state->bread_ready = true;

                        check_ready();
                    });
            });
    }

private:
    net::io_context& io_;

    Store store_;

    std::shared_ptr<GasCooker> gas_cooker_ =
        std::make_shared<GasCooker>(io_);
};