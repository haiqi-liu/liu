#ifndef CATCH_CONFIG_MAIN
#  define CATCH_CONFIG_MAIN
#endif

#include "atm.hpp"
#include "catch.hpp"

#include <fstream>   // for ifstream/ofstream used in helpers
#include <string>

/////////////////////////////////////////////////////////////////////////////////////////////
//                             Helper Definitions //
/////////////////////////////////////////////////////////////////////////////////////////////

bool CompareFiles(const std::string& p1, const std::string& p2) {
  std::ifstream f1(p1);
  std::ifstream f2(p2);

  if (f1.fail() || f2.fail()) {
    return false;  // file problem
  }

  std::string f1_read;
  std::string f2_read;
  while (f1.good() || f2.good()) {
    f1 >> f1_read;
    f2 >> f2_read;
    if (f1_read != f2_read || (f1.good() && !f2.good()) ||
        (!f1.good() && f2.good()))
      return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Test Cases
/////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("RegisterAccount creates account + empty transaction list; CheckBalance works", "[register][balance]") {
  Atm atm;

  atm.RegisterAccount(12345678u, 1234u, "Sam Sepiol", 300.30);
  auto& accounts = atm.GetAccounts();
  auto& txs = atm.GetTransactions();

  REQUIRE(accounts.contains({12345678u, 1234u}));
  REQUIRE(txs.contains({12345678u, 1234u}));
  REQUIRE(txs[{12345678u, 1234u}].empty());

  // Member function call
  REQUIRE( atm.CheckBalance(12345678u, 1234u) == Approx(300.30) );

  // Member function call
  REQUIRE_THROWS_AS(atm.CheckBalance(11111111u, 2222u), std::invalid_argument);
}

TEST_CASE("RegisterAccount rejects duplicate (card,pin)", "[register][duplicate]") {
  Atm atm;
  atm.RegisterAccount(22222222u, 2222u, "Alice", 100.0);

  REQUIRE_THROWS_AS(
      atm.RegisterAccount(22222222u, 2222u, "Alice-again", 50.0),
      std::invalid_argument);
}

TEST_CASE("WithdrawCash: normal withdraw updates balance and records transaction", "[withdraw]") {
  Atm atm;
  atm.RegisterAccount(33333333u, 3333u, "Bob", 200.0);

  atm.WithdrawCash(33333333u, 3333u, 40.5);
  REQUIRE( atm.CheckBalance(33333333u, 3333u) == Approx(159.5) );

  auto& txs = atm.GetTransactions();
  auto& v = txs[{33333333u, 3333u}];
  REQUIRE_FALSE(v.empty());

  bool has_withdraw = false, has_amount = false;
  for (const auto& line : v) {
    if (line.find("Withdrawal") != std::string::npos) has_withdraw = true;
    if (line.find("$40.50") != std::string::npos || line.find("$40.5") != std::string::npos) has_amount = true;
  }
  REQUIRE(has_withdraw);
  REQUIRE(has_amount);
}

TEST_CASE("WithdrawCash: negative amount should throw invalid_argument", "[withdraw][negative]") {
  Atm atm;
  atm.RegisterAccount(44444444u, 4444u, "Carol", 100.0);
  REQUIRE_THROWS_AS(atm.WithdrawCash(44444444u, 4444u, -1.0), std::invalid_argument);
}

TEST_CASE("WithdrawCash: overdraft should throw runtime_error", "[withdraw][overdraft]") {
  Atm atm;
  atm.RegisterAccount(55555555u, 5555u, "Dan", 50.0);
  REQUIRE_THROWS_AS(atm.WithdrawCash(55555555u, 5555u, 50.01), std::runtime_error);

  REQUIRE( atm.CheckBalance(55555555u, 5555u) == Approx(50.0) );
}

TEST_CASE("WithdrawCash: non-existent account should throw invalid_argument", "[withdraw][missing]") {
  Atm atm;
  REQUIRE_THROWS_AS(atm.WithdrawCash(99999999u, 9999u, 1.0), std::invalid_argument);
}

TEST_CASE("DepositCash: normal deposit updates balance and records transaction", "[deposit]") {
  Atm atm;
  atm.RegisterAccount(66666666u, 6666u, "Eve", 10.0);

  atm.DepositCash(66666666u, 6666u, 123.45);
  REQUIRE( atm.CheckBalance(66666666u, 6666u) == Approx(133.45) );

  auto& txs = atm.GetTransactions();
  auto& v = txs[{66666666u, 6666u}];
  REQUIRE_FALSE(v.empty());
  bool has_deposit = false, has_amount = false;
  for (const auto& line : v) {
    if (line.find("Deposit") != std::string::npos) has_deposit = true;
    if (line.find("$123.45") != std::string::npos) has_amount = true;
  }
  REQUIRE(has_deposit);
  REQUIRE(has_amount);
}

TEST_CASE("DepositCash: negative amount should throw invalid_argument", "[deposit][negative]") {
  Atm atm;
  atm.RegisterAccount(77777777u, 7777u, "Frank", 0.0);
  REQUIRE_THROWS_AS(atm.DepositCash(77777777u, 7777u, -100.0), std::invalid_argument);
}

TEST_CASE("DepositCash: non-existent account should throw invalid_argument", "[deposit][missing]") {
  Atm atm;
  REQUIRE_THROWS_AS(atm.DepositCash(88888888u, 8888u, 10.0), std::invalid_argument);
}

TEST_CASE("PrintLedger prints header + transactions in expected format", "[ledger]") {
  Atm atm;
  atm.RegisterAccount(12345678u, 1234u, "Sam Sepiol", 300.30);

  auto& txs = atm.GetTransactions();
  txs[{12345678u, 1234u}].push_back(
      "Withdrawal - Amount: $200.40, Updated Balance: $99.90");
  txs[{12345678u, 1234u}].push_back(
      "Deposit - Amount: $40000.00, Updated Balance: $40099.90");
  txs[{12345678u, 1234u}].push_back(
      "Deposit - Amount: $32000.00, Updated Balance: $72099.90");

  const std::string out = "ledger_out.txt";
  atm.PrintLedger(out, 12345678u, 1234u);

  const std::string exp = "ledger_exp.txt";
  {
    std::ofstream os(exp);
    REQUIRE(os.is_open());
    os << "Name: Sam Sepiol\n";
    os << "Card Number: 12345678\n";
    os << "PIN: 1234\n";
    os << "----------------------------\n";
    os << "Withdrawal - Amount: $200.40, Updated Balance: $99.90\n";
    os << "Deposit - Amount: $40000.00, Updated Balance: $40099.90\n";
    os << "Deposit - Amount: $32000.00, Updated Balance: $72099.90\n";
  }

  REQUIRE( CompareFiles(exp, out) );
}

TEST_CASE("PrintLedger: non-existent account throws invalid_argument", "[ledger][missing]") {
  Atm atm;
  REQUIRE_THROWS_AS(atm.PrintLedger("x.txt", 11111111u, 2222u), std::invalid_argument);
}