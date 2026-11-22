
#include "EmergencyQueue.h"
#include <stdexcept>

bool EmergencyCompare::operator()(
    const EmergencyRequest& a,
    const EmergencyRequest& b
) const {
    return static_cast<int>(a.severity) <
           static_cast<int>(b.severity);
}

void EmergencyQueue::addEmergency(
    const EmergencyRequest& request
) {
    requests.push(request);
}

EmergencyRequest EmergencyQueue::getNextEmergency() {
    if (requests.empty()) {
        throw std::runtime_error("No emergencies in queue.");
    }

    EmergencyRequest next = requests.top();
    requests.pop();
    return next;
}

bool EmergencyQueue::isEmpty() const {
    return requests.empty();
}

int EmergencyQueue::size() const {
    return static_cast<int>(requests.size());
}