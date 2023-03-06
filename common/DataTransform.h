#ifndef DATA_TRANSFORM_H
#define DATA_TRANSFORM_H

/** \file
 *
 * Defines analyses to extract the functions called a function.
 */

#include <map>
#include <string>
#include <set>
#include "iostream"
#include <unordered_set>
#include "Halide.h"
#include "BoundEstimate.h"
#include "DataOP.h"
namespace Halide {
namespace Internal {

using std::map;
using std::pair;
using std::string;
static bool data_transform_debug = false;
namespace {
/* Find all the internal halide calls in an expr */
template <typename T>
class DataTransformer : public IRVisitor {
private:
    std::string target_;
    Func replace_func_;
    T op_;
public:
    //map<string, Function> calls;
    using IRVisitor::visit;
    DataTransformer() = default;
    DataTransformer(Func target_func,Func func,T op){
        replace_func_ = func;
        target_ = target_func.name();
        op_ = op;
    }
    void visit(const Add *add) override {
         IRVisitor::visit(add);
         Add *node = const_cast<Add *>(reinterpret_cast<const Add *>(add)) ;
         if (const Halide::Internal::Call *v= node->a.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->a = replace_func_(expr);
             }
         }
         if (const Halide::Internal::Call *v= node->b.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {   
                 auto expr = v->args;
                 op_(expr);
                 node->b = replace_func_(expr);
             }
         }
     }
     void visit(const Mul *mul) override {
         IRVisitor::visit(mul);
         Mul *node = const_cast<Mul *>(reinterpret_cast<const Mul *>(mul)) ;
         if (const Halide::Internal::Call *v= node->a.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->a = replace_func_(expr);
             }
         }
         if (const Halide::Internal::Call *v= node->b.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->b = replace_func_(expr);
             }
         }
     }
     void visit(const Sub *sub) override {
         IRVisitor::visit(sub);
         Sub *node = const_cast<Sub *>(reinterpret_cast<const Sub *>(sub)) ;
         if (const Halide::Internal::Call *v= node->a.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->a = replace_func_(expr);
             }
         }
         if (const Halide::Internal::Call *v= node->b.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->b = replace_func_(expr);
             }
         }
     }
     void visit(const Div *div) override {
         IRVisitor::visit(div);
         Div *node = const_cast<Div *>(reinterpret_cast<const Div *>(div)) ;
         if (const Halide::Internal::Call *v= node->a.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->a = replace_func_(expr);
             }
         }
         if (const Halide::Internal::Call *v= node->b.as<Halide::Internal::Call>()) {
             if (v->name==target_)
             {
                 auto expr = v->args;
                 op_(expr);
                 node->b = replace_func_(expr);
             }
         }
     }
};
}  // namespace
template<typename T>
void data_transform_impl(Function f,Func target)
{
    std::map<std::string, Function> env = build_environment({f});
    Function target_function;
    bool flag = false;
    for (auto iter = env.begin();iter!=env.end();iter++){
        if (iter->first==target.name()){
            flag = true;
            break;
        }
    }
    if (!flag)
    {
        std::cout<<"erro file name:"<<target.name()<<std::endl;
        return;
    }
        
    T op;
    std::string new_name = target.name()+op.name();
    Func newfunc(new_name);
    op(newfunc,target);
    
    for (auto iter = env.begin();iter!=env.end();iter++){
        if (iter->first!=target.name()){
            DataTransformer<T> tf(target,newfunc,op);
            iter->second.accept(&tf);
        }
    }
}
void data_transform(std::vector<Function> &outputs,Func target,DataTransformMethod method){
    for (Function &o : outputs) {
        switch( method)
        {
            case DataTransformMethod::REORDER:
                data_transform_impl<ReorderOP>(o, target);
                break;
            case DataTransformMethod::INTERLEAVE:
                data_transform_impl<InterleaveOP>(o,target);
                break;
            case DataTransformMethod::SPLITY:
                data_transform_impl<SplitYOP>(o,target);
                break;
            default:
                //TODO error?
                break;
        }   
    }
}
double compute_layout_cost_impl(std::vector<Function> &outputs);
std::vector<Function> deep_copy(std::vector<Function> &func)
{
    std::map<std::string, Function> env = build_environment(func);
    auto copy_pair = deep_copy(func,env);
    return copy_pair.first;
}
std::vector<Function> auto_data_transform(const Pipeline &p)
{
    
    std::vector<Function> outputs;
    for (Func f : p.outputs()) {
        outputs.push_back(f.function());
    }
    string use_data_transform = get_env_variable("HL_USE_DATA_TRANSFORM");
    if (use_data_transform!="True")
         return outputs;
    if (data_transform_debug)
        std::cout<<"\ndata transform info:\n"<<std::endl;
    
    string data_debug = get_env_variable("HL_DEBUG_DATA_TRANSFORM");
    if (data_debug=="True")
        data_transform_debug =true;
    else
        data_transform_debug =false;
    double min_cost=compute_layout_cost_impl(outputs);;
    bool flag=true;
    auto best_outputs = outputs;
    std::vector<string> schedule;
    while (flag)
    {
        flag=false;
        //std::vector<Function> temp_outputs = deep_copy(best_outputs);
        std::vector<Function> copy_outputs = deep_copy(best_outputs);
        std::vector<Function> inputs = find_input_function(copy_outputs);
        int best_input_idx=-1;
        Halide::Internal::DataTransformMethod best_op;
        for (unsigned int i=0;i<inputs.size();i++)
        {
            Func inp = Func(inputs[i]);
            data_transform(copy_outputs,inp,Halide::Internal::DataTransformMethod::INTERLEAVE);
            double interleave_cost = compute_layout_cost_impl(copy_outputs);
            if (interleave_cost<min_cost)
            {
                //std::cout<<"smaller"<<std::endl;
                //data_transform(temp_outputs,inp,Halide::Internal::DataTransformMethod::INTERLEAVE);
                best_op = Halide::Internal::DataTransformMethod::INTERLEAVE;
                best_input_idx = i;
                min_cost=interleave_cost;
                flag=true;
            }
            copy_outputs = deep_copy(best_outputs);
        }
        //inputs = find_input_function(copy_outputs);
        for (unsigned int i=0;i<inputs.size();i++)
        {
            Func inp = Func(inputs[i]);
            data_transform(copy_outputs,inp,Halide::Internal::DataTransformMethod::REORDER);
            double reorder_cost = compute_layout_cost_impl(copy_outputs);
            if (reorder_cost<min_cost)
            {
                best_op = Halide::Internal::DataTransformMethod::REORDER;
                best_input_idx = i;
                min_cost=reorder_cost;
                flag=true;
            }
            copy_outputs = deep_copy(best_outputs);
        }
        //inputs = find_input_function(copy_outputs);
        for (unsigned int i=0;i<inputs.size();i++)
        {
            Func inp = Func(inputs[i]);
            data_transform(copy_outputs,inp,Halide::Internal::DataTransformMethod::SPLITY);
            double split_cost = compute_layout_cost_impl(copy_outputs);
            if (split_cost<min_cost)
            {
                best_op = Halide::Internal::DataTransformMethod::SPLITY;
                best_input_idx = i;
                flag=true;
                
            }
            copy_outputs = deep_copy(best_outputs);
        }
        std::vector<Function> best_inputs = find_input_function(best_outputs); 
        if (flag)
        {
            Func inp = Func(best_inputs[best_input_idx]);
            data_transform(best_outputs,inp,best_op);
            schedule.push_back(scheduler_name(best_op)+"->"+inp.name());
        }
    }
    if (data_transform_debug)
    {
        if (schedule.size()==0)
        {
            std::cout<<"the best schedule is Original one,we do not change the data layout"<<std::endl;

        }else{
            std::cout<<"the best schedule is:"<<std::endl;
            for(auto str:schedule)
            {
                std::cout<<" "<<str;
            }
            std::cout<<std::endl;
            std::cout<<"the min cost is :"<<min_cost<<std::endl;
        }
        
    }
    
    return best_outputs;
}
const int ORDER_COST_LIST[3] = {512*32,512*32*2,512*32*32}; 
double sigmoid(double x)
{
    return 1.0/(1+std::exp(-x));
}
double compute_order_cost(const Definition  &def,std::string function_name,const std::string &target, std::map<std::string,std::pair<int,int> >& bounds)
{
    //func(x,y) = input_a(k,y)*input_b(x,k)
    const StageSchedule &s = def.schedule();
    std::vector<std::string> loop_order;
    for (auto d:s.dims())
    {
        if (d.var.find("outermost")==string::npos)
        {
            loop_order.emplace_back(d.var);
        }
    }
    std::vector<Expr> rvalue = def.values();
    std::unordered_map<std::string,int> args_to_idx;
    for (unsigned int i=0;i<loop_order.size();i++)
    {
        args_to_idx[loop_order[i]] = i;
    }
    FindOrder findorder(target,args_to_idx);
    // def.accept(&findlooporder);
    for (Expr expr:rvalue)
    {
        expr.accept(&findorder);
    }
    double res=0.0;
    if (findorder.IsMisorder())
    {
        std::string first_var = target+".0";
        std::string second_var = target+".1";
        int range_first = bounds[first_var].second-bounds[first_var].first+1;
        int range_second = bounds[second_var].second-bounds[second_var].first+1;
        int range_fastest = range_first*range_second;
        if (range_fastest>ORDER_COST_LIST[0]&&range_fastest<=ORDER_COST_LIST[1])
        {
            res=0.0000075;
            int offset=0;
            for (unsigned int i=0;i<loop_order.size();i++)
            {
                double range = 1.0;
                if (bounds.find(loop_order[i])!=bounds.end())
                {
                    //RDom
                    offset++;
                    range = 1.0*(bounds[loop_order[i]].second-bounds[loop_order[i]].first+1);
                }else
                {
                    std::string var = function_name +"."+std::to_string(i-offset);
                    range = 1.0*(bounds[var].second-bounds[var].first+1);
                }  
                res*= range;
            }
        }else if (range_fastest>ORDER_COST_LIST[1]&&range_fastest<=ORDER_COST_LIST[2])
        {
            res=0.000095;
            int offset=0;
            for (unsigned int i=0;i<loop_order.size();i++)
            {
                double range = 1.0;
                if (bounds.find(loop_order[i])!=bounds.end())
                {
                    //RDom
                    offset++;
                    range = 1.0*(bounds[loop_order[i]].second-bounds[loop_order[i]].first+1);
                }else
                {
                    std::string var = function_name +"."+std::to_string(i-offset);
                    range = 1.0*(bounds[var].second-bounds[var].first+1);
                }  
                res*= range;
            }
        }else if (range_fastest>ORDER_COST_LIST[2])
        {
            res=0.000395;
            int offset=0;
            for (unsigned int i=0;i<loop_order.size();i++)
            {
                double range = 1.0;
                if (bounds.find(loop_order[i])!=bounds.end())
                {
                    //RDom
                    offset++;
                    range = 1.0*(bounds[loop_order[i]].second-bounds[loop_order[i]].first+1);
                }else
                {
                    std::string var = function_name +"."+std::to_string(i-offset);
                    range = 1.0*(bounds[var].second-bounds[var].first+1);
                    //std::cout<<"shape var:"<<var<<" range:"<<range<<std::endl;
                }  
                res*= range;
            }
        }
    }
    return res;
}
double compute_use_distance(const Definition &def,const std::string &target,std::map<std::string,std::pair<int,int> >& bounds)
{
    
    const StageSchedule &s = def.schedule();
    std::vector<std::string> loop_order;
    for (auto d:s.dims())
    {
        if (d.var.find("outermost")==string::npos)
        {
            loop_order.emplace_back(d.var);
        }
    }

    std::vector<Expr> rvalue = def.values();
    std::unordered_map<std::string,int> args_to_idx;
    for (unsigned int i=0;i<loop_order.size();i++)
    {
        args_to_idx[loop_order[i]] = i;
    }
    FindOrder findorder(target,args_to_idx);
    // def.accept(&findlooporder);
    for (Expr expr:rvalue)
    {
        expr.accept(&findorder);
    }
    auto orders = findorder.get_orders();
    
    std::string fastest_var = loop_order[0];
    int idx=-1;
    for (unsigned int i=0;i<orders.size();i++)
    {
        if (orders[i].find(fastest_var)!=orders[i].end())
        {
            idx=i;
            break;
        }
    }
    double res=0.1;
    if(idx==-1){
        //TODO
        std::cout<<"warning:var "<<fastest_var<<" not in the function"<<std::endl;
        res =0.0;
    }
    if (idx==0){
        // the fastest var are also in the first order of target function
        res= 0.0;
    }
    
    std::string func_name = findorder.get_target(); 
    for (int i=0;i<idx;i++)
    {
        std::string bound_name = func_name+"."+std::to_string(i);
        double range=1.0*(bounds[bound_name].second-bounds[bound_name].first+1);
        res *= range;
    }
    return res;

}
double compute_layout_cost_impl(std::vector<Function> &outputs){
    std::vector<Function> inputs = find_input_function(outputs);
    double cost=0;
    for (Function &func:inputs){
        std::string target_name = func.name();
        std::map<std::string,std::pair<int,int> > bounds;
        estimate_bound(outputs,bounds);
        std::vector<std::pair<Definition,std::string>> consumers = find_definition(outputs,target_name);
        for (std::pair<Definition,std::string> &consumer:consumers)
        {
            double cost_order = compute_order_cost(consumer.first,consumer.second,target_name,bounds);
            double cost_distance = compute_use_distance(consumer.first,target_name,bounds);
            cost += cost_order+cost_distance;
        }
    }
    return cost;
}
}  // namespace Internal
}  // namespace Halide

#endif
