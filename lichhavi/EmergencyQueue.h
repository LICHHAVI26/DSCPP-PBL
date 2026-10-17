
#ifndef EMERGENCY_QUEUE_H
#define EMERGENCY_QUEUE_H

#include "Models.h"
#include <queue>
#include <vector>

struct EmergencyCompare {
    bool operator()(const EmergencyRequest& a,
                    const EmergencyRequest& b) const;
};

class EmergencyQueue {
private:
    std::priority_queue<
        EmergencyRequest,
        std::vector<EmergencyRequest>,
        EmergencyCompare
    > requests;

public:
    void addEmergency(const EmergencyRequest& request);
    EmergencyRequest getNextEmergency();
    bool isEmpty() const;
    int size() const;
};

#endif