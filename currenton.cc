#include <iostream>
#include <future>
#include <stack>

/* This code introduces the "currenton". It is like a per-thread
 * singleton that can be changed in a stack fashion when making
 * function calls. This construct can be useful to provide e.g. memory
 * pools, loggers and clocks (or other functions that are typically
 * provided as singletons) for only a part of a program.
 *
 * Consider the example of testing a class with a method that waits
 * for a thread to finish or timeout. This is difficult to test, since
 * the actual time for the thread to finish or for the timeout to
 * trigger depends on the thread scheduler, which can vary depending
 * on the run environment. One solution is to let the class take a
 * clock (say Clock) as an argument at construction and provide a fake
 * or mock at time of testing. However, then the clock has to be
 * passed around in the code and in particular be known to the
 * constructor of the class. A currenton allows the class to use
 * Currenton<Clock>::get() to get the current clock. This could be set
 * as to fake or a mock clock if the code is currently being
 * tested.
 */

/* This is an example of an integer playing the role of the clock
 * above.
 */
class Integer {
public:
  Integer(int value) : value(value) { };

  int getValue() const {
    return value;
  }
  
private:
  int value;
};

/* The Currenton provides a stack of objects, of which the top-most
 * one is current and returned on get(). It provides a function
 * "makeCurrent" that pushes a new object on the stack and makes a
 * call to a given function. When the thread returns from the function
 * (or throws), the topmost object is popped from the stack.
 */
template<typename T>
class Currenton
{
public:
  static T& get()
  {
    if (!getCurrenton().stack.empty()) {
      return getCurrenton().stack.top();
    }
    else {
      throw std::runtime_error("No current object");
    }
  }

  static void makeCurrent(T&& t, std::function<void()> f) {
    getCurrenton().stack.push(std::move(t));
    try {
      f();
    } catch(...) {
      getCurrenton().stack.pop();
      throw;
    }
    getCurrenton().stack.pop();
  }
  
private:
  static Currenton& getCurrenton()
  {
    thread_local Currenton currenton;
    return currenton;
  }
  
  Currenton() { std::cout << "Creating thread currenton." << std::endl; };
  ~Currenton() { std::cout << "Destroying thread currenton." << std::endl; };
  
  Currenton(const Currenton&) = delete;
  Currenton& operator=(const Currenton&) = delete;
  Currenton(Currenton&&) = delete;
  Currenton& operator=(Currenton&&) = delete;

  std::stack<T> stack;
};

void incrementNicely()
{
  int currentValue = Currenton<Integer>::get().getValue();

  if (currentValue < 20) {
    Currenton<Integer>::makeCurrent({currentValue+1}, incrementNicely);
  }

  std::cout << "Before call: " << currentValue << ". After call: " << Currenton<Integer>::get().getValue() << std::endl;
  
}

void incrementButFail()
{
  int currentValue = Currenton<Integer>::get().getValue();

  if (currentValue == 15) {
    throw std::runtime_error("Oh no.");
  }
  
  if (currentValue < 20) {
    try {
      Currenton<Integer>::makeCurrent({currentValue+1}, incrementButFail);
    }
    catch(...) {
      std::cout << "Before call: " << currentValue << ". After exception: " << Currenton<Integer>::get().getValue() << std::endl;
      throw;
    }
  }
}

int main()
{
  // Check that we have access to the same integer before and after call
  Currenton<Integer>::makeCurrent({10}, incrementNicely);

  // Check that we have access to the same integer before and after throw
  try {
    Currenton<Integer>::makeCurrent({10}, incrementButFail);
  }
  catch (std::runtime_error e) {
    std::cout << "Recursion failed with \"" << e.what() << "\"" << std::endl;
  }

  // Check that threads have access to their own stack of integers
  auto a = std::async([](){Currenton<Integer>::makeCurrent({18}, incrementNicely);});
  auto b = std::async([](){Currenton<Integer>::makeCurrent({18}, incrementNicely);});
  auto c = std::async([](){Currenton<Integer>::makeCurrent({18}, incrementNicely);});
  auto d = std::async([](){Currenton<Integer>::makeCurrent({18}, incrementNicely);});

  a.get();
  b.get();
  c.get();
  d.get();
  
  // Check that we have to set an object to start with
  try {
    incrementNicely();
  }
  catch (std::runtime_error e) {
    std::cout << "Failed with \"" << e.what() << "\"" << std::endl;
  }

}
