# lattice::assembly Developer Guide

## 1. Overview
`lattice::assembly` is a high-performance C++ application framework designed for low-latency systems. It provides a structured way to build applications composed of decoupled components that communicate via ring buffers (Ethers) and are executed by pinned threads (Dispatchers).

## 2. Core Concepts

### 2.1 Ether (The Ring Buffer)
The **Ether** is the communication backbone. It is a lock-free, single-producer, multi-consumer ring buffer.
*   **Role:** Stores messages (POD types) for consumption by components.
*   **Key Feature:** Messages are typed and accessed via a `Cursor`.
*   **Usage:** Messages are allocated directly in the buffer (`allocMsg`) and then committed (`commitMsg`) to become visible to consumers.

### 2.2 Component (The Logic Unit)
A **Component** encapsulates a specific piece of application logic.
*   **Role:** Processes specific message types.
*   **Structure:** Must inherit from `assembly::ComponentBase`.
*   **Message Handling:** Implements `processMsg(const MsgType&)` for every message type it subscribes to.
*   **Lifecycle:** Has hooks for `initialize`, `start`, `stop`, and batch processing events (`processBatchEnd`).

### 2.3 Dispatcher (The Execution Engine)
A **Dispatcher** is a thread that drives a set of Components.
*   **Role:** Reads messages from an Ether, checks timers/IO, and invokes Component handlers.
*   **Traits:** Can be configured with traits (e.g., `DispatcherWithTimer`, `DispatcherWithEpoll`) to enable features like timing or network IO.
*   **Pinning:** Can be pinned to a specific CPU core for consistent latency.

### 2.4 Compartment & Assembly
*   **Compartment:** A grouping of one Ether and one or more Dispatchers that read from it.
*   **Assembly:** The top-level container that manages the lifecycle (init/start/stop) of all Compartments and holds the Application Context.

## 3. Creating an Application

Follow these steps to build a new application:

### Step 3.1: Define Messages
Define the data structures (PODs) that will be passed between components.
```cpp
struct Order { int id; double price; };
struct Trade { int id; double price; };
```

### Step 3.2: Define Components
Create components by inheriting from `ComponentBase` and implementing message handlers.

```cpp
// Define the input message list for this component
using MyInputList = type::type_list<Order>;

struct MyStrategy : public assembly::ComponentBase<MyStrategy, "MyStrategy", MyInputList, MyTraits> {
    using ComponentBase::ComponentBase;

    void processMsg(const Order& msg) {
        // Business logic here
        // Sending a message:
        auto& trade = allocMsg<Trade>();
        trade.id = msg.id;
        commitMsg(trade);
    }
};
```

### Step 3.3: Configure the Assembly
Define the types that make up your application structure using `typedef` or `using`.

1.  **Context:** Define your application context (inheriting from `assembly::Context`).
2.  **Ether:** Define the Ether type with the list of all possible messages.
    ```cpp
    using AllMessages = type::type_list<Order, Trade>;
    using MyEther = assembly::Ether<"MainEther", AllMessages, 1024>;
    ```
3.  **Dispatcher:** Define the Dispatcher type, associating it with the Ether and list of Components.
    ```cpp
    using MyComponents = type::type_list<MyStrategy>;
    using MyDispatcher = assembly::Dispatcher<"MainDisp", MyContext, MyEther, MyComponents>;
    ```
4.  **Assembly:** Tie it all together.
    ```cpp
    using MyCompartment = assembly::Compartment<MyContext, MyEther, MyDispatcher>;
    using MyAssembly = assembly::Assembly<MyContext, MyCompartment>;
    ```

### Step 3.4: Main Execution
Instantiate and start the assembly.

```cpp
int main() {
    MyContext context("MyApp", "config.json");
    MyAssembly app(context);
    
    app.initialize();
    app.start();
    
    // Main thread can wait or perform other tasks
    while(true) sleep(1);
    
    app.stop();
    return 0;
}
```

## 4. Configuration

The framework uses a JSON-based configuration system via `assembly::Config`.
*   **Ethers:** Configure shared memory paths and initialization flags.
*   **Attributes:** Components can read configuration values at runtime using `getAttribute<Type>()`.

## 5. Performance Best Practices

1.  **Core Pinning:** Dedicate isolated cores to Dispatchers to minimize context switching and cache pollution.
2.  **Batch Processing:** Use `processBatchEnd` for logic that doesn't need to run per-message (e.g., flushing logs or bulk updates).
3.  **Zero-Copy:** The framework uses zero-copy message passing. Always allocate messages via `allocMsg` to construct them directly in the ring buffer.
4.  **POD Messages:** Ensure all messages are Plain Old Data (POD) types to ensure safe storage in shared memory ring buffers.
