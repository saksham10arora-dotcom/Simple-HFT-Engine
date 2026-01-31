#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <vector>
#include "Order.hpp"

class OrderBook
{
private:
    std::map<double, std::vector<Order *>, std::greater<double>> bids;
    std::map<double, std::vector<Order *>> asks;

public:
    void addOrder(Order *order)
    {
        if (order->side == Side::Buy)
        {
            bids[order->price].push_back(order);
        }
        else
        {
            asks[order->price].push_back(order);
        }
    }

    void printBBO()
    {
        if (!bids.empty() && !asks.empty())
        {
            std::cout << "Best Bid: " << bids.begin()->first
                      << " | Best Ask: " << asks.begin()->first
                      << " | Spread: " << (asks.begin()->first - bids.begin()->first) << "\n";
        }
    }

    // High-speed match (No Printing!)
    void match()
    {
        while (!bids.empty() && !asks.empty())
        {
            auto itBid = bids.begin();
            auto itAsk = asks.begin();

            if (itBid->first >= itAsk->first)
            {
                // In a real HFT engine, this is where a "Trade Execution" event fires
                itBid->second.pop_back();
                itAsk->second.pop_back();

                if (itBid->second.empty())
                    bids.erase(itBid);
                if (itAsk->second.empty())
                    asks.erase(itAsk);
            }
            else
            {
                break;
            }
        }
    }
};

#endif