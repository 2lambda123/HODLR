// File used in CI testing

#include "HODLR_Tree.hpp"
#include "HODLR_Matrix.hpp"
#include "KDTree.hpp"

// Derived class of HODLR_Matrix which is ultimately
// passed to the HODLR_Tree class:
class Kernel_Gaussian : public HODLR_Matrix 
{

private:
    Mat x;

public:

    // Constructor:
    Kernel_Gaussian(int N) : HODLR_Matrix(N) 
    {
        x = (Mat::Random(N, 1)).real();
        // This is being sorted to ensure that we get
        // optimal low rank structure:
        getKDTreeSorted(x, 0);
    };
    
    // In this example, we are illustrating usage using
    // the gaussian kernel:
    dtype getMatrixEntry(int i, int j) 
    {
        // Value on the diagonal:
        if(i == j)
        {
            return 100;
        }
        
        // Otherwise:
        else
        {   
            dtype R = 0;
            R = abs(x(i, 0) - x(j, 0));
            return exp(-R * R);
        }
    }

    // Destructor:
    ~Kernel_Gaussian() {};
};

class Random_Matrix : public HODLR_Matrix 
{

private:
    Mat x;

public:

    // Constructor:
    Random_Matrix(int N) : HODLR_Matrix(N) 
    {
        x = (Mat::Random(N, N)).real().cwiseAbs();
        x = 0.5 * (x + x.transpose());
        x = x + N * N * Mat::Identity(N, N);
    };
    
    // In this example, we are illustrating usage using
    // the gaussian kernel:
    dtype getMatrixEntry(int i, int j) 
    {
        return x(i, j);
    }

    // Destructor:
    ~Random_Matrix() {};
};

int main(int argc, char* argv[]) 
{
    // We are considering a matrix of size 1000 X 1000:
    int N             = 1000;
    // Size of matrix at leaf level is 100 X 100
    int M             = 100;
    // Number of levels in the tree:
    int n_levels       = log(N / M) / log(2);
    // We take the tolerance of problem as 10^{-12}
    double tolerance  = pow(10, -12);
    // Dummy check to ensure that zeros are returned when HODLR_Matrix is directly used:
    HODLR_Matrix* K_dummy = new HODLR_Matrix(N);
    assert(K_dummy->getMatrixEntry(rand() % N, rand() % N) == 0);
    delete K_dummy;

    // Declaration of HODLR_Matrix object that abstracts data in Matrix:
    Kernel_Gaussian* K_gaussian = new Kernel_Gaussian(N);
    // Througout, we have ensured that the error in the method is lesser than 
    // 10^4 X tolerance that was requested for ACA. It is not always necessary
    // for the error for all methods to be exactly equal to the tolerance of ACA 
    // Random Matrix to multiply with
    Mat x = (Mat::Random(N, 1)).real();
    // Stores the result after multiplication:
    Mat y_fast, b_fast;

    // Testing fast factorization:
    HODLR_Tree* T = new HODLR_Tree(n_levels, tolerance, K_gaussian);
    bool is_sym = false;
    bool is_pd = false;
    T->assembleTree(is_sym, is_pd);
    T->printTreeDetails();
	T->plotTree();
    
    b_fast      = T->matmatProduct(x);
    Mat B       = K_gaussian->getMatrix(0, 0, N, N);
    Mat b_exact = B * x;
    assert((b_fast-b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    T->factorize();
    Mat x_fast;
    x_fast = T->solve(b_exact);
    assert((x_fast - x).norm() / (x.norm()) < 1e4 * tolerance);


    dtype log_det;
    Eigen::PartialPivLU<Mat> lu;
    lu.compute(B);
    log_det = 0.0;
    for(int i = 0; i < lu.matrixLU().rows(); i++)
    {
        log_det += log(lu.matrixLU()(i,i));
    }

    dtype log_det_hodlr = T->logDeterminant();
    assert(fabs(1 - fabs(log_det_hodlr/log_det)) < 1e4 * tolerance);
    delete T;

    // Testing fast symmetric factorization:
    T = new HODLR_Tree(n_levels, tolerance, K_gaussian);
    is_sym = true;
    is_pd = true;
    T->assembleTree(is_sym, is_pd);
    T->printTreeDetails();
	T->plotTree();

    b_fast = T->matmatProduct(x);
    // Computing the relative error in the solution obtained:
    assert((b_fast-b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    T->factorize();
    x_fast = T->solve(b_exact);
    assert((x_fast - x).norm() / (x.norm()) < 1e4 * tolerance);

    y_fast = T->symmetricFactorTransposeProduct(x);
    b_fast = T->symmetricFactorProduct(y_fast);

    assert((b_fast - b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    Eigen::LLT<Mat> llt;
    llt.compute(B);
    log_det = 0.0;
    for(int i = 0; i < llt.matrixL().rows(); i++)
    {
        log_det += log(llt.matrixL()(i,i));
    }
    log_det *= 2;

    log_det_hodlr = T->logDeterminant();
    assert(fabs(1 - fabs(log_det_hodlr/log_det)) < 1e4 * tolerance);

    // Getting the symmetric factor:
    Mat W  = T->getSymmetricFactor();
    Mat Wt = W.transpose();

    assert((Wt.colPivHouseholderQr().solve(W.colPivHouseholderQr().solve(b_exact)) - x).cwiseAbs().maxCoeff() < 1e4 * tolerance);
    delete T;
    delete K_gaussian;

    // This is a tough check of the low rank approximation, so we set a lower tolerance for this:
    tolerance  = pow(10, -7);
    Random_Matrix* random_matrix = new Random_Matrix(N);
    // Testing fast factorization:
    T = new HODLR_Tree(n_levels, tolerance, random_matrix);
    is_sym = false;
    is_pd = false;
    T->assembleTree(is_sym, is_pd);
    T->printTreeDetails();
	// T->plotTree();
    
    b_fast  = T->matmatProduct(x);
    B       = random_matrix->getMatrix(0, 0, N, N);
    b_exact = B * x;
    assert((b_fast-b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    T->factorize();
    x_fast = T->solve(b_exact);
    assert((x_fast - x).norm() / (x.norm()) < 1e4 * tolerance);

    lu.compute(B);
    log_det = 0.0;
    for(int i = 0; i < lu.matrixLU().rows(); i++)
    {
        log_det += log(lu.matrixLU()(i,i));
    }

    log_det_hodlr = T->logDeterminant();
    assert(fabs(1 - fabs(log_det_hodlr/log_det)) < 1e4 * tolerance);
    delete T;

    // Testing fast symmetric factorization:
    T = new HODLR_Tree(n_levels, tolerance, random_matrix);
    is_sym = true;
    is_pd = true;
    T->assembleTree(is_sym, is_pd);
    T->printTreeDetails();
	// T->plotTree();

    b_fast = T->matmatProduct(x);
    // Computing the relative error in the solution obtained:
    assert((b_fast-b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    T->factorize();
    x_fast = T->solve(b_exact);
    assert((x_fast - x).norm() / (x.norm()) < 1e4 * tolerance);

    y_fast = T->symmetricFactorTransposeProduct(x);
    b_fast = T->symmetricFactorProduct(y_fast);

    assert((b_fast - b_exact).norm() / (b_exact.norm()) < 1e4 * tolerance);

    llt.compute(B);
    log_det = 0.0;
    for(int i = 0; i < llt.matrixL().rows(); i++)
    {
        log_det += log(llt.matrixL()(i,i));
    }
    log_det *= 2;

    log_det_hodlr = T->logDeterminant();
    assert(fabs(1 - fabs(log_det_hodlr/log_det)) < 1e4 * tolerance);

    // Getting the symmetric factor:
    W  = T->getSymmetricFactor();
    Wt = W.transpose();

    assert((Wt.colPivHouseholderQr().solve(W.colPivHouseholderQr().solve(b_exact)) - x).cwiseAbs().maxCoeff() < 1e4 * tolerance);
    delete T;
    delete random_matrix;

    return 0;
}
