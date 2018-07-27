#ifndef PolynomialTableCorrectRounded_H
#define PolynomialTableCorrectRounded_H

#include "../Table.hpp"

namespace flopoco{

	class PolynomialTableCorrectRounded : public Table
	{
	public:
		PolynomialTableCorrectRounded(Target* target, int p1, int w1);
	
		~PolynomialTableCorrectRounded() override;
  	
		mpz_class function(int x) override;
	
		//  int check_accuracy(int wF);
	
		int    double2input(double x) override;
	
		double input2double(int x) override;
	
		mpz_class double2output(double x) override;
	
		double output2double(mpz_class x) override;
	
		double maxMulOut;
		double minMulOut;
	
	private:
	
	
	};
}
#endif

